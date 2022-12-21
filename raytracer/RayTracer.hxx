#pragma once
#include "../Ray.hxx"
#include "../Filter.hxx"
#include "glm/gtx/norm.hpp"

namespace RayTracer::Config {
    inline auto enableShadow = false;
    inline auto enableReflection = false;
    inline auto enableRefraction = false;
    inline auto enableTextureMap = false;
    inline auto enableParallelism = false;
    inline auto enableSuperSample = false;
    inline auto enableAcceleration = false;
    inline auto enableDepthOfField = false;
}

namespace RayTracer {
    auto Draw(auto Canvas, auto&& RenderedImage) {
        auto FloatingPointToUInt8 = [](auto x) { return std::clamp(static_cast<int>(255 * x), 0, 255); };
        for (auto y : Range{ RenderedImage[0].Height })
            for (auto x : Range{ RenderedImage[0].Width })
                Canvas[y][x] = { FloatingPointToUInt8(RenderedImage[0][y][x]), FloatingPointToUInt8(RenderedImage[1][y][x]), FloatingPointToUInt8(RenderedImage[2][y][x]) };
    }
    auto BilinearDownsample(auto&& Image) {
        auto DownsampledImage = Filter::Frame{ Image[0].Height / 2, Image[0].Width / 2, Image.PlaneCount };
        auto HorizontalKernel = [](auto Center) { return 0.25 * Center[0][-1] + 0.5 * Center[0][0] + 0.25 * Center[0][1]; };
        auto VerticalKernel = [](auto Center) { return 0.25 * Center[-1][0] + 0.5 * Center[0][0] + 0.25 * Center[1][0]; };
        for (auto ResampledImage = VerticalKernel * (HorizontalKernel * Image); auto y : Range{ DownsampledImage[0].Height })
            for (auto x : Range{ DownsampledImage[0].Width })
                for (auto c : Range{ DownsampledImage.PlaneCount })
                    DownsampledImage[c][y][x] = (ResampledImage[c][2 * y][2 * x] + ResampledImage[c][2 * y + 1][2 * x] + ResampledImage[c][2 * y][2 * x + 1] + ResampledImage[c][2 * y + 1][2 * x + 1]) / 4;
        return DownsampledImage.Finalize();
    }
    [[gnu::flatten]] auto Render(auto Height, auto Width, auto SupersamplingExponent, auto&& Metadata) {
        SupersamplingExponent = Config::enableSuperSample ? SupersamplingExponent : 0;
        Height <<= SupersamplingExponent;
        Width <<= SupersamplingExponent;

        using CameraType = struct {
            glm::vec3 Position;
            glm::vec3 Look;
            glm::vec3 Up;
            double HeightAngle;
            double FocalLength;
        };
        auto Camera = CameraType{
            .Position = glm::vec3{ Metadata.cameraData.pos },
            .Look = glm::normalize(glm::vec3{ Metadata.cameraData.look }),
            .Up = glm::normalize(glm::vec3{ Metadata.cameraData.up }),
            .HeightAngle = glm::radians(Metadata.cameraData.heightAngle),
            .FocalLength = 0.1
        };
        auto ProjectToWorldSpace = ViewPlane::ConfigureProjectorFromScreenSpaceToWorldSpace(Camera, Width, Height);

        auto LightRecords = Metadata.lights | [](auto&& x)->Lights::Ǝ {
            auto QuadraticAttenuation = [](auto&& Coefficients) {
                return [=](auto&& Distance) {
                    return std::min(1 / (Coefficients[0] + Coefficients[1] * Distance + Coefficients[2] * Distance * Distance), 1.f);
                };
            };
            if (x.type == LightType::LIGHT_POINT)
                return Lights::Point(glm::vec3{ x.pos }, glm::vec3{ x.color }, QuadraticAttenuation(x.function));
            else if (x.type == LightType::LIGHT_DIRECTIONAL)
                return Lights::Directional(glm::normalize(glm::vec3{ x.dir }), glm::vec3{ x.color });
            else if (x.type == LightType::LIGHT_SPOT)
                return Lights::Spot(glm::vec3{ x.pos }, glm::normalize(glm::vec3{ x.dir }), glm::radians(x.angle), glm::radians(x.penumbra), glm::vec3{ x.color }, QuadraticAttenuation(x.function));
            else
                throw std::runtime_error{ "Unrecognized light type detected!" };
        };

        auto ObjectRecords = Metadata.shapes | [](auto&& x) {
            using MaterialType = struct {
                glm::vec3 AmbientCoefficients;
                glm::vec3 DiffuseCoefficients;
                glm::vec3 SpecularCoefficients;
                glm::vec3 ReflectionCoefficients;
                glm::vec3 TransparencyCoefficients;
                double SpecularExponent;
                double η;
                bool IsReflective;
                bool IsTransparent;
            };
            auto&& [Primitive, ObjectTransformation] = x;
            auto ImplicitFunction = [&]()->ImplicitFunctions::Ǝ {
                if (Primitive.type == PrimitiveType::PRIMITIVE_CUBE)
                    return ObjectTransformation * ImplicitFunctions::Standard::Cube;
                else if (Primitive.type == PrimitiveType::PRIMITIVE_SPHERE)
                    return ObjectTransformation * ImplicitFunctions::Standard::Sphere;
                else if (Primitive.type == PrimitiveType::PRIMITIVE_CYLINDER)
                    return ObjectTransformation * ImplicitFunctions::Standard::Cylinder;
                else if (Primitive.type == PrimitiveType::PRIMITIVE_CONE)
                    return ObjectTransformation * ImplicitFunctions::Standard::Cone;
                else
                    throw std::runtime_error{ "Unrecognized primitive type detected!" };
            }();
            auto Material = MaterialType{
                .AmbientCoefficients = glm::vec3{ Primitive.material.cAmbient },
                .DiffuseCoefficients = glm::vec3{ Primitive.material.cDiffuse },
                .SpecularCoefficients = glm::vec3{ Primitive.material.cSpecular },
                .ReflectionCoefficients = glm::vec3{ Primitive.material.cReflective },
                .TransparencyCoefficients = glm::vec3{ Primitive.material.cTransparent },
                .SpecularExponent = Primitive.material.shininess,
                .η = Primitive.material.ior,
                .IsReflective = Config::enableReflection && glm::l1Norm(glm::vec3{ Primitive.material.cReflective }) > 1e-16,
                .IsTransparent = Config::enableRefraction && glm::l1Norm(glm::vec3{ Primitive.material.cTransparent }) > 1e-16
            };
            return std::tuple{ ImplicitFunction, Material };
        };

        auto ObstructionRecords = Config::enableShadow ? ObjectRecords | [](auto&& x) { return std::get<0>(x); } : std::vector<ImplicitFunctions::Ǝ>{};
        auto IlluminationModel = Illuminations::WhittedModel(LightRecords, ObstructionRecords);
        auto SupersampledImage = Filter::Frame{ Height, Width, 3 };

        Illuminations::Ka = Metadata.globalData.ka;
        Illuminations::Kd = Metadata.globalData.kd;
        Illuminations::Ks = Metadata.globalData.ks;
        Illuminations::Kt = Metadata.globalData.kt;

        for (auto y : Range{ Height })
            for (auto x : Range{ Width })
                for (auto AccumulatedIntensity = Ray::Trace(Camera.Position, glm::normalize(ProjectToWorldSpace(x, y) - Camera.Position), IlluminationModel, ObjectRecords, 1); auto c : Range{ 3 })
                    SupersampledImage[c][y][x] = AccumulatedIntensity[c];

        auto RenderedImage = SupersampledImage.Finalize();
        for (auto _ : Range{ SupersamplingExponent })
            RenderedImage = BilinearDownsample(RenderedImage);

        return RenderedImage;
    }
}