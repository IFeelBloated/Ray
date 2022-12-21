#pragma once
#include "glm_fix.hxx"

namespace ViewPlane {
	auto ConfigureProjectorFromScreenSpaceToWorldSpace(auto&& Camera, auto Width, auto Height) {
		auto V = 2 * Camera.FocalLength * std::tan(Camera.HeightAngle / 2);
		auto U = V * Width / Height;
		auto TransformationToWorldSpace = [&] {
			auto w = -Camera.Look;
			auto v = glm::normalize(Camera.Up - glm::dot(Camera.Up, w) * w);
			auto u = glm::cross(v, w);
			return glm::translate(Camera.Position) * glm::mat4{
				u.x, u.y, u.z, 0.f,
				v.x, v.y, v.z, 0.f,
				w.x, w.y, w.z, 0.f,
				0.f, 0.f, 0.f, 1.f
			};
		}();
		return [=](auto x, auto y) {
			auto NormalizedX = (x + 0.5) / Width - 0.5;
			auto NormalizedY = 0.5 - (y + 0.5) / Height;
			auto HomogenizedCoordinates = TransformationToWorldSpace * glm::vec4{ U * NormalizedX, V * NormalizedY, -Camera.FocalLength, 1. };
			return glm::vec3{ HomogenizedCoordinates };
		};
	}
}

namespace Ray {
	constexpr auto NoIntersection = std::numeric_limits<double>::infinity();
	inline auto SelfIntersectionDisplacement = 1e-3f;
	inline auto RecursiveTracingDepth = 6;

	auto Reflect(auto&& IncomingDirection, auto&& SurfaceNormal) {
		return glm::normalize(IncomingDirection + 2 * glm::dot(SurfaceNormal, -IncomingDirection) * SurfaceNormal);
	}
	auto Refract(auto&& IncomingDirection, auto&& SurfaceNormal, auto η) {
		auto cosθ1 = glm::dot(-SurfaceNormal, IncomingDirection);
		if (auto Discriminant = 1 - η * η * (1 - cosθ1 * cosθ1); Discriminant < 0)
			return std::tuple{ true, glm::vec3{} };
		else
			return std::tuple{ false, glm::normalize(η * IncomingDirection + (η * cosθ1 - std::sqrt(Discriminant)) * SurfaceNormal) };
	}
	auto Intersect(auto&& EyePoint, auto&& RayDirection, auto& ObjectRecords) {
		auto IntersectionRecords = ObjectRecords | [&](auto& x) {
			auto& [ImplicitFunction, Material] = x;
			return ImplicitFunction(EyePoint, RayDirection) + std::tuple<decltype(Material)&>{ Material };
		};
		return *std::min_element(IntersectionRecords.begin(), IntersectionRecords.end(), [](auto&& x, auto&& y) { return std::get<0>(x) < std::get<0>(y); });
	}
	auto DetectOcclusion(auto&& EyePoint, auto&& RayDirection, auto DistanceLimit, auto&& ObstructionRecords) {
		for (auto&& ImplicitFunction : ObstructionRecords)
			if (auto&& [t, _] = ImplicitFunction(EyePoint + SelfIntersectionDisplacement * RayDirection, RayDirection); t < DistanceLimit)
				return true;
		return false;
	}
	auto Trace(auto&& EyePoint, auto&& RayDirection, auto&& IlluminationModel, auto&& ObjectRecords, auto RecursionDepth)->glm::vec3 {
		if (auto&& [t, SurfaceNormal, SurfaceMaterial] = Intersect(EyePoint, RayDirection, ObjectRecords); RecursionDepth < RecursiveTracingDepth && t != NoIntersection) {
			auto IntersectionPosition = EyePoint + t * RayDirection;
			auto AccumulateReflectedIntensity = [&] {
				auto ReflectedRayDirection = Reflect(RayDirection, SurfaceNormal);
				return Trace(IntersectionPosition + SelfIntersectionDisplacement * ReflectedRayDirection, ReflectedRayDirection, IlluminationModel, ObjectRecords, RecursionDepth + 1);
			};
			auto AccumulateRefractedIntensity = [&] {
				auto [RefractionNormal, η] = glm::dot(RayDirection, SurfaceNormal) > 0 ? std::tuple{ -SurfaceNormal, SurfaceMaterial.η } : std::tuple{ SurfaceNormal, 1 / SurfaceMaterial.η };
				if (auto [TotalInternalReflection, RefractedRayDirection] = Refract(RayDirection, RefractionNormal, η); TotalInternalReflection == false)
					return Trace(IntersectionPosition + SelfIntersectionDisplacement * RefractedRayDirection, RefractedRayDirection, IlluminationModel, ObjectRecords, RecursionDepth + 1);
				return glm::vec3{ 0, 0, 0 };
			};
			auto EstimateReflectance = [&] {
				auto cosθi = glm::dot(RayDirection, SurfaceNormal);
				auto [η1, η2] = cosθi > 0 ? std::tuple{ 1., SurfaceMaterial.η } : std::tuple{ SurfaceMaterial.η, 1. };
				if (auto sinθt = η2 / η1 * std::sqrt(std::max(0., 1. - cosθi * cosθi)); sinθt >= 1)
					return 1.;
				else {
					auto cosθt = std::sqrt(std::max(0., 1. - sinθt * sinθt));
					auto RootOfRs = (η1 * std::abs(cosθi) - η2 * cosθt) / (η1 * std::abs(cosθi) + η2 * cosθt);
					auto RootOfRp = (η2 * std::abs(cosθi) - η1 * cosθt) / (η2 * std::abs(cosθi) + η1 * cosθt);
					return (RootOfRs * RootOfRs + RootOfRp * RootOfRp) / 2;
				}
			};
			auto [Reflectance, ReflectedIntensity, RefractedIntensity] = [&] {
				if (SurfaceMaterial.IsReflective && SurfaceMaterial.IsTransparent)
					return std::tuple{ EstimateReflectance(), AccumulateReflectedIntensity(), AccumulateRefractedIntensity() };
				else if (SurfaceMaterial.IsReflective)
					return std::tuple{ 1., AccumulateReflectedIntensity(), glm::vec3{ 0, 0, 0 } };
				else if (SurfaceMaterial.IsTransparent)
					return std::tuple{ 0., glm::vec3{ 0, 0, 0 }, AccumulateRefractedIntensity() };
				else
					return std::tuple{ 0., glm::vec3{ 0, 0, 0 }, glm::vec3{ 0, 0, 0 } };
			}();
			return IlluminationModel(SurfaceMaterial, IntersectionPosition, SurfaceNormal, EyePoint, Reflectance * ReflectedIntensity, (1 - Reflectance) * RefractedIntensity);
		}
		return { 0, 0, 0 };
	}
}

namespace Lights {
	using Signature = auto(const glm::vec3&)->std::tuple<double, glm::vec3, glm::vec3>;
	using Ǝ = std::function<Signature>;

	auto Point(auto&& Position, auto&& Color, auto&& AttenuationFunction) {
		return [=](auto&& SurfacePosition) {
			auto Displacement = SurfacePosition - Position;
			auto [Distance, Direction] = std::tuple{ std::sqrt(glm::dot(Displacement, Displacement)), glm::normalize(Displacement) };
			return std::tuple{ Distance, Direction, AttenuationFunction(Distance) * Color };
		};
	}
	auto Directional(auto&& Direction, auto&& Color) {
		return [=](auto&&) { return std::tuple{ std::numeric_limits<double>::infinity(), Direction, Color }; };
	}
	auto Spot(auto&& Position, auto&& Axis, auto θ, auto Penumbra, auto&& Color, auto&& AttenuationFunction) {
		return [=, Umbra = θ - Penumbra](auto&& SurfacePosition) {
			auto Displacement = SurfacePosition - Position;
			auto [Distance, Direction] = std::tuple{ std::sqrt(glm::dot(Displacement, Displacement)), glm::normalize(Displacement) };
			if (auto φ = std::acos(glm::dot(Direction, Axis)); φ > θ)
				return std::tuple{ Distance, Direction, 0.f * Color };
			else if (auto Attenuation = AttenuationFunction(Distance); φ <= Umbra)
				return std::tuple{ Distance, Direction, Attenuation * Color };
			else {
				auto Falloff = [&] {
					auto α = (φ - Umbra) / Penumbra;
					return -2 * std::pow(α, 3) + 3 * std::pow(α, 2);
				}();
				return std::tuple{ Distance, Direction, (1 - Falloff) * Attenuation * Color };
			}
		};
	}
}

namespace Illuminations {
	auto [Ka, Kd, Ks, Kt] = std::array{ 1.f, 1.f, 1.f, 1.f };

	auto Diffuse(auto&& LightDirection, auto&& SurfaceNormal, auto&& LightColor, auto&& DiffuseCoefficients) {
		return std::max(glm::dot(SurfaceNormal, -LightDirection), 0.f) * DiffuseCoefficients * LightColor;
	}
	auto Specular(auto&& LightDirection, auto&& SurfaceNormal, auto&& EyeDirection, auto&& LightColor, auto&& SpecularCoefficients, auto SpecularExponent) {
		return std::pow(std::max(glm::dot(Ray::Reflect(LightDirection, SurfaceNormal), EyeDirection), 0.f), SpecularExponent) * SpecularCoefficients * LightColor;
	}
	auto WhittedModel(auto& LightRecords, auto& ObstructionRecords) {
		return [&](auto&& Material, auto&& SurfacePosition, auto&& SurfaceNormal, auto&& EyePoint, auto&& ReflectedIntensity, auto&& RefractedIntensity) {
			auto AccumulatedIntensity = Ka * Material.AmbientCoefficients;
			for (auto&& Light : LightRecords)
				if (auto [LightDistance, LightDirection, LightColor] = Light(SurfacePosition); Ray::DetectOcclusion(SurfacePosition, -LightDirection, LightDistance, ObstructionRecords) == false) {
					AccumulatedIntensity += Diffuse(LightDirection, SurfaceNormal, LightColor, Kd * Material.DiffuseCoefficients);
					AccumulatedIntensity += Specular(LightDirection, SurfaceNormal, glm::normalize(EyePoint - SurfacePosition), LightColor, Ks * Material.SpecularCoefficients, Material.SpecularExponent);
				}
			return AccumulatedIntensity + Ks * Material.ReflectionCoefficients * ReflectedIntensity + Kt * Material.TransparencyCoefficients * RefractedIntensity;
		};
	}
}

namespace ImplicitFunctions {
	using Signature = auto(const glm::vec3&, const glm::vec3&)->std::tuple<double, glm::vec3>;
	using Ǝ = std::function<Signature>;

	constexpr auto ε = std::numeric_limits<double>::min();
}

namespace { // implicit function operators are globally visible
	auto operator+(auto&& ImplicitFunction, auto&& OtherImplicitFunction) requires requires {
		{ ImplicitFunction(glm::vec3{}, glm::vec3{}) }->SubtypeOf<std::tuple<double, glm::vec3>>;
		{ OtherImplicitFunction(glm::vec3{}, glm::vec3{}) }->SubtypeOf<std::tuple<double, glm::vec3>>;
	} {
		return [=](auto&& EyePoint, auto&& RayDirection) {
			return std::min(
				ImplicitFunction(EyePoint, RayDirection),
				OtherImplicitFunction(EyePoint, RayDirection),
				[](auto&& x, auto&& y) { return std::get<0>(x) < std::get<0>(y); }
			);
		};
	}
	auto operator*(SubtypeOf<glm::mat4> auto&& ObjectTransformation, auto&& ImplicitFunction) requires requires {
		{ ImplicitFunction(glm::vec3{}, glm::vec3{}) }->SubtypeOf<std::tuple<double, glm::vec3>>;
	} {
		return [=, InverseTransformation = glm::inverse(ObjectTransformation), NormalTransformation = glm::inverse(glm::transpose(glm::mat3{ ObjectTransformation }))](auto&& EyePoint, auto&& RayDirection) {
			auto [ObjectSpaceEyePoint, ObjectSpaceRayDirection] = [&] {
				auto [HomogenizedEyePoint, HomogenizedRayDirection] = std::tuple{ glm::vec4{ EyePoint, 1 }, glm::vec4{ RayDirection, 0 } };
				return std::tuple{ glm::vec3{ InverseTransformation * HomogenizedEyePoint }, glm::vec3{ InverseTransformation * HomogenizedRayDirection } };
			}();
			if (auto [t, SurfaceNormal] = ImplicitFunction(ObjectSpaceEyePoint, ObjectSpaceRayDirection); t != Ray::NoIntersection)
				return std::tuple{ t, glm::normalize(NormalTransformation * SurfaceNormal) };
			return std::tuple{ Ray::NoIntersection, glm::vec3{} };
		};
	}
}

namespace ImplicitFunctions::Solvers {
	auto Quadratic(auto&& CoefficientGenerator, auto&& NormalGenerator, auto&& Constraint) {
		return [=](auto&& EyePoint, auto&& RayDirection) {
			auto [a, b, c] = CoefficientGenerator(EyePoint, RayDirection);
			auto ConstrainExistingRoot = [&](auto Root) {
				auto IntersectionPosition = glm::vec3{ EyePoint + Root * RayDirection };
				return Constraint(IntersectionPosition.x, IntersectionPosition.y, IntersectionPosition.z) ? std::tuple{ Root, NormalGenerator(IntersectionPosition) } : std::tuple{ Ray::NoIntersection, glm::vec3{} };
			};
			if (auto Discriminant = b * b - 4 * a * c; std::abs(a) <= ε || Discriminant < 0)
				return std::tuple{ Ray::NoIntersection, glm::vec3{} };
			else if (auto SmallerRoot = (-b - std::sqrt(Discriminant)) / (2. * a); SmallerRoot >= 0)
				return ConstrainExistingRoot(SmallerRoot);
			else if (auto LargerRoot = (-b + std::sqrt(Discriminant)) / (2. * a); LargerRoot >= 0)
				return ConstrainExistingRoot(LargerRoot);
			return std::tuple{ Ray::NoIntersection, glm::vec3{} };
		};
	}
	auto Planar(auto&& NormalGenerator, auto MainAxis, auto ...SupportAxes) {
		return [=](auto PlaneCoordinate, auto&& Constraint) {
			return [=, SurfaceNormal = NormalGenerator(PlaneCoordinate)](auto&& EyePoint, auto&& RayDirection) {
				if (auto t = (PlaneCoordinate - EyePoint[MainAxis]) / RayDirection[MainAxis]; std::abs(RayDirection[MainAxis]) > ε && Constraint(EyePoint[SupportAxes] + t * RayDirection[SupportAxes]...))
					return t >= 0 ? std::tuple{ t, SurfaceNormal } : std::tuple{ Ray::NoIntersection, SurfaceNormal };
				return std::tuple{ Ray::NoIntersection, SurfaceNormal };
			};
		};
	}
	inline auto XZPlane = Planar([](auto PlaneCoordinate) { return PlaneCoordinate >= 0 ? glm::vec3{ 0, 1, 0 } : glm::vec3{ 0, -1, 0 }; }, 1, 0, 2);
	inline auto XYPlane = Planar([](auto PlaneCoordinate) { return PlaneCoordinate >= 0 ? glm::vec3{ 0, 0, 1 } : glm::vec3{ 0, 0, -1 }; }, 2, 0, 1);
	inline auto YZPlane = Planar([](auto PlaneCoordinate) { return PlaneCoordinate >= 0 ? glm::vec3{ 1, 0, 0 } : glm::vec3{ -1, 0, 0 }; }, 0, 1, 2);
}

namespace ImplicitFunctions::Standard::Constraints {
	inline auto BoundedPlane = [](auto x, auto y) {
		return -0.5 <= x && x <= 0.5 && -0.5 <= y && y <= 0.5;
	};
	inline auto CircularPlane = [](auto x, auto y) {
		return x * x + y * y <= 0.5 * 0.5;
	};
	inline auto BoundedHeight = [](auto, auto y, auto) {
		return -0.5 <= y && y <= 0.5;
	};
}

namespace ImplicitFunctions::Standard {
	inline auto Cube = Solvers::YZPlane(-0.5, Constraints::BoundedPlane) + Solvers::YZPlane(0.5, Constraints::BoundedPlane) +
		Solvers::XZPlane(0.5, Constraints::BoundedPlane) + Solvers::XZPlane(-0.5, Constraints::BoundedPlane) +
		Solvers::XYPlane(0.5, Constraints::BoundedPlane) + Solvers::XYPlane(-0.5, Constraints::BoundedPlane);
	inline auto Sphere = Solvers::Quadratic(
		[](auto&& EyePoint, auto&& RayDirection) {
			return std::tuple{
				RayDirection.x * RayDirection.x + RayDirection.y * RayDirection.y + RayDirection.z * RayDirection.z,
				2 * (RayDirection.x * EyePoint.x + RayDirection.y * EyePoint.y + RayDirection.z * EyePoint.z),
				EyePoint.x * EyePoint.x + EyePoint.y * EyePoint.y + EyePoint.z * EyePoint.z - 0.25
			};
		},
		[](auto&& IntersectionPosition) { return glm::normalize(IntersectionPosition); },
		[](auto...) { return true; }
	);
	inline auto Cylinder = Solvers::Quadratic(
		[](auto&& EyePoint, auto&& RayDirection) {
			return std::tuple{
				RayDirection.x * RayDirection.x + RayDirection.z * RayDirection.z,
				2 * (RayDirection.x * EyePoint.x + RayDirection.z * EyePoint.z),
				EyePoint.x * EyePoint.x + EyePoint.z * EyePoint.z - 0.25
			};
		},
		[](auto&& IntersectionPosition) { return glm::normalize(glm::vec3{ IntersectionPosition.x, 0, IntersectionPosition.z }); },
		Constraints::BoundedHeight
	) + Solvers::XZPlane(0.5, Constraints::CircularPlane) + Solvers::XZPlane(-0.5, Constraints::CircularPlane);
	inline auto Cone = Solvers::Quadratic(
		[](auto&& EyePoint, auto&& RayDirection) {
			return std::tuple{
				RayDirection.x * RayDirection.x + RayDirection.z * RayDirection.z - 0.25 * RayDirection.y * RayDirection.y,
				2 * (RayDirection.x * EyePoint.x + RayDirection.z * EyePoint.z) - 0.5 * RayDirection.y * EyePoint.y + 0.25 * RayDirection.y,
				EyePoint.x * EyePoint.x + EyePoint.z * EyePoint.z - 0.25 * EyePoint.y * EyePoint.y + 0.25 * EyePoint.y - 0.0625
			};
		},
		[](auto&& IntersectionPosition) { return glm::normalize(glm::vec3{ 2 * IntersectionPosition.x, 0.25 - 0.5 * IntersectionPosition.y, 2 * IntersectionPosition.z }); },
		Constraints::BoundedHeight
	) + Solvers::XZPlane(-0.5, Constraints::CircularPlane);
}