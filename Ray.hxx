#pragma once
#include "Infrastructure.hxx"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/transform.hpp"

namespace ViewPlane {
	auto ConfigureProjectorFromScreenSpaceToWorldSpace(auto&& Camera, auto Height, auto Width) {
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
		return [=](auto y, auto x) {
			auto NormalizedX = (x + 0.5) / Width - 0.5;
			auto NormalizedY = 0.5 - (y + 0.5) / Height;
			return TransformationToWorldSpace * glm::vec4{ U * NormalizedX, V * NormalizedY, -Camera.FocalLength, 1. };
		};
	}
}

namespace Ray {
	constexpr auto NoIntersection = std::numeric_limits<double>::infinity();
	inline auto SelfIntersectionDisplacement = 1e-3f;
	inline auto RecursiveTracingDepth = 4;

	auto Reflect(auto&& IncomingDirection, auto&& SurfaceNormal) {
		return glm::normalize(IncomingDirection + 2 * glm::dot(SurfaceNormal, -IncomingDirection) * SurfaceNormal);
	}
	auto Refract(auto&& IncomingDirection, auto&& SurfaceNormal, auto η) {
		auto cosθ1 = glm::dot(-SurfaceNormal, IncomingDirection);
		if (auto Discriminant = 1 - η * η * (1 - cosθ1 * cosθ1); Discriminant < 0)
			return std::tuple{ true, glm::vec4{} };
		else
			return std::tuple{ false, glm::normalize(static_cast<float>(η) * IncomingDirection + static_cast<float>(η * cosθ1 - std::sqrt(Discriminant)) * SurfaceNormal) };
	}
	auto Intersect(auto&& ImplicitFunction, auto&& ObjectTransformation, auto&& EyePoint, auto&& RayDirection) {
		auto [ObjectSpaceEyePoint, ObjectSpaceRayDirection] = [&] {
			auto InverseTransformation = glm::inverse(ObjectTransformation);
			return std::array{ InverseTransformation * EyePoint, InverseTransformation * RayDirection };
		}();
		if (auto [t, SurfaceNormal] = ImplicitFunction(ObjectSpaceEyePoint, ObjectSpaceRayDirection); t != NoIntersection)
			return std::tuple{ t, glm::vec4{ glm::normalize(glm::inverse(glm::transpose(glm::mat3{ ObjectTransformation })) * SurfaceNormal), 0 } };
		return std::tuple{ NoIntersection, glm::vec4{} };
	}
	auto DetectOcclusion(auto&& EyePoint, auto&& RayDirection, auto&& ObjectRecords) {
		for (auto&& [ImplicitFunction, ObjectTransformation, _] : ObjectRecords)
			if (auto&& [t, __] = Intersect(ImplicitFunction, ObjectTransformation, EyePoint + SelfIntersectionDisplacement * RayDirection, RayDirection); t != NoIntersection)
				return true;
		return false;
	}
	auto Trace(auto&& EyePoint, auto&& RayDirection, auto&& IlluminationModel, auto&& ObjectRecords, auto RecursionDepth)->glm::vec3 {
		using ObjectRecordType = std::remove_cvref_t<decltype(*std::begin(ObjectRecords))>;
		auto IntersectionRecord = std::tuple{ NoIntersection, glm::vec4{}, glm::vec4{}, static_cast<ObjectRecordType*>(nullptr) };
		auto& [IntersectionDistance, IntersectionPosition, IntersectionSurfaceNormal, PointerToIntersectedObjectRecord] = IntersectionRecord;
		for (auto& x : ObjectRecords) {
			auto& [ImplicitFunction, ObjectTransformation, _] = x;
			if (auto [t, SurfaceNormal] = Intersect(ImplicitFunction, ObjectTransformation, EyePoint, RayDirection); t < IntersectionDistance)
				IntersectionRecord = std::tuple{ t, EyePoint + static_cast<float>(t) * RayDirection, SurfaceNormal, &x };
		}
		if (RecursionDepth < RecursiveTracingDepth && IntersectionDistance != NoIntersection) {
			auto& [_, __, Material] = *PointerToIntersectedObjectRecord;
			auto ReflectedIntensity = [&] {
				auto ReflectedRayDirection = Reflect(RayDirection, IntersectionSurfaceNormal);
				return Trace(IntersectionPosition + SelfIntersectionDisplacement * ReflectedRayDirection, ReflectedRayDirection, IlluminationModel, ObjectRecords, RecursionDepth + 1);
			}();
			auto RefractedIntensity = [&] {
				auto [RefractionNormal, η] = glm::dot(RayDirection, IntersectionSurfaceNormal) > 0 ? std::tuple{ -IntersectionSurfaceNormal, Material.η } : std::tuple{ IntersectionSurfaceNormal, 1 / Material.η };
				if (auto [TotalInternalReflection, RefractedRayDirection] = Refract(RayDirection, RefractionNormal, η); TotalInternalReflection == false)
					return Trace(IntersectionPosition + SelfIntersectionDisplacement * RefractedRayDirection, RefractedRayDirection, IlluminationModel, ObjectRecords, RecursionDepth + 1);
				return glm::vec3{ 0, 0, 0 };
			}();
			auto Reflectance = [&] {
				auto cosθi = glm::dot(RayDirection, IntersectionSurfaceNormal);
				auto [η1, η2] = cosθi > 0 ? std::tuple{ 1., Material.η } : std::tuple{ Material.η, 1. };
				if (auto sinθt = η2 / η1 * std::sqrt(std::max(0., 1. - cosθi * cosθi)); sinθt >= 1)
					return 1.;
				else {
					auto cosθt = std::sqrt(std::max(0., 1. - sinθt * sinθt));
					auto RootOfRs = (η1 * std::abs(cosθi) - η2 * cosθt) / (η1 * std::abs(cosθi) + η2 * cosθt);
					auto RootOfRp = (η2 * std::abs(cosθi) - η1 * cosθt) / (η2 * std::abs(cosθi) + η1 * cosθt);
					return (RootOfRs * RootOfRs + RootOfRp * RootOfRp) / 2;
				}
			}();
			return IlluminationModel(Material, IntersectionPosition, IntersectionSurfaceNormal, EyePoint, static_cast<float>(Reflectance) * ReflectedIntensity, static_cast<float>(1 - Reflectance) * RefractedIntensity);
		}
		return { 0, 0, 0 };
	}
}

namespace Lights {
	using Signature = auto(const glm::vec4&)->std::tuple<glm::vec4, glm::vec3>;
	using Ǝ = std::function<Signature>;

	auto Point(auto&& Position, auto&& Color, auto&& AttenuationFunction) {
		return [=](auto&& SurfacePosition) {
			auto Displacement = SurfacePosition - Position;
			return std::tuple{ glm::normalize(Displacement), AttenuationFunction(std::sqrt(glm::dot(Displacement, Displacement))) * Color };
		};
	}
	auto Directional(auto&& Direction, auto&& Color) {
		return [=](auto&&) { return std::tuple{ Direction, Color }; };
	}
	auto Spot(auto&& Position, auto&& Axis, auto θ, auto Penumbra, auto&& Color, auto&& AttenuationFunction) {
		return [=, Umbra = θ - Penumbra](auto&& SurfacePosition) {
			auto Displacement = SurfacePosition - Position;
			auto Direction = glm::normalize(Displacement);
			if (auto φ = std::acos(glm::dot(Direction, Axis)); φ > θ)
				return std::tuple{ Direction, 0.f * Color };
			else if (auto Attenuation = AttenuationFunction(std::sqrt(glm::dot(Displacement, Displacement))); φ <= Umbra)
				return std::tuple{ Direction, Attenuation * Color };
			else {
				auto Falloff = [&] {
					auto α = (φ - Umbra) / Penumbra;
					return -2 * std::pow(α, 3) + 3 * std::pow(α, 2);
				}();
				return std::tuple{ Direction, static_cast<float>(1 - Falloff) * Attenuation * Color };
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
		return static_cast<float>(std::pow(std::max(glm::dot(Ray::Reflect(LightDirection, SurfaceNormal), EyeDirection), 0.f), SpecularExponent)) * SpecularCoefficients * LightColor;
	}
	auto WhittedModel(auto& LightRecords, auto& ObjectRecords) {
		return [&](auto&& Material, auto&& SurfacePosition, auto&& SurfaceNormal, auto&& EyePoint, auto&& ReflectedIntensity, auto&& RefractedIntensity) {
			auto AccumulatedIntensity = Ka * Material.AmbientCoefficients;
			for (auto&& Light : LightRecords)
				if (auto [LightDirection, LightColor] = Light(SurfacePosition); Ray::DetectOcclusion(SurfacePosition, -LightDirection, ObjectRecords) == false) {
					AccumulatedIntensity += Diffuse(LightDirection, SurfaceNormal, LightColor, Kd * Material.DiffuseCoefficients);
					AccumulatedIntensity += Specular(LightDirection, SurfaceNormal, glm::normalize(EyePoint - SurfacePosition), LightColor, Ks * Material.SpecularCoefficients, Material.SpecularExponent);
				}
			return AccumulatedIntensity + Ks * Material.ReflectionCoefficients * ReflectedIntensity + Kt * Material.TransparencyCoefficients * RefractedIntensity;
		};
	}
}

namespace ImplicitFunctions {
	using Signature = auto(const glm::vec4&, const glm::vec4&)->std::tuple<double, glm::vec3>;
	using Ǝ = std::function<Signature>;

	constexpr auto ε = std::numeric_limits<double>::min();
}

namespace ImplicitFunctions::Solvers {
	auto Quadratic(auto&& EyePoint, auto&& RayDirection, auto a, auto b, auto c, auto&& Constraint) {
		auto ConstrainExistingRoot = [&](auto Root) {
			auto IntersectionPosition = EyePoint + static_cast<float>(Root) * RayDirection;
			return Constraint(IntersectionPosition.x, IntersectionPosition.y, IntersectionPosition.z) ? std::tuple{ Root, glm::vec3{ IntersectionPosition } } : std::tuple{ Ray::NoIntersection, glm::vec3{} };
		};
		if (auto Discriminant = b * b - 4 * a * c; std::abs(a) <= ε || Discriminant < 0)
			return std::tuple{ Ray::NoIntersection, glm::vec3{} };
		else if (auto SmallerRoot = (-b - std::sqrt(Discriminant)) / (2. * a); SmallerRoot >= 0)
			return ConstrainExistingRoot(SmallerRoot);
		else if (auto LargerRoot = (-b + std::sqrt(Discriminant)) / (2. * a); LargerRoot >= 0)
			return ConstrainExistingRoot(LargerRoot);
		return std::tuple{ Ray::NoIntersection, glm::vec3{} };
	}
	constexpr auto CanonicalPlanar(auto MainAxis, auto ...SupportAxes) {
		return [=](auto&& EyePoint, auto&& RayDirection, auto PlaneCoordinate, auto&& Constraint) {
			if (auto t = (PlaneCoordinate - EyePoint[MainAxis]) / RayDirection[MainAxis]; std::abs(RayDirection[MainAxis]) > ε && Constraint(EyePoint[SupportAxes] + t * RayDirection[SupportAxes]...))
				return t >= 0 ? t : Ray::NoIntersection;
			return Ray::NoIntersection;
		};
	}
	constexpr auto XZPlane = CanonicalPlanar(1, 0, 2);
	constexpr auto XYPlane = CanonicalPlanar(2, 0, 1);
	constexpr auto YZPlane = CanonicalPlanar(0, 1, 2);
}

namespace ImplicitFunctions::StandardConstraints {
	constexpr auto BoundedPlane = [](auto x, auto y) {
		return -0.5 <= x && x <= 0.5 && -0.5 <= y && y <= 0.5;
	};
	constexpr auto CircularPlane = [](auto x, auto y) {
		return x * x + y * y <= 0.5 * 0.5;
	};
	constexpr auto BoundedHeight = [](auto, auto y, auto) {
		return -0.5 <= y && y <= 0.5;
	};
}

namespace ImplicitFunctions::ImplementationDetail {
	auto DetermineNearestIntersection(auto&& CurrentIntersectionRecord, auto&& CandidateIntersectionRecord, auto&& ...Arguments) {
		auto& [CurrentIntersectionDistance, _] = CurrentIntersectionRecord;
		if (auto& [CandidateIntersectionDistance, __] = CandidateIntersectionRecord; CandidateIntersectionDistance < CurrentIntersectionDistance)
			CurrentIntersectionRecord = CandidateIntersectionRecord;
		if constexpr (sizeof...(Arguments) != 0)
			return DetermineNearestIntersection(CurrentIntersectionRecord, Arguments...);
		else
			return CurrentIntersectionRecord;
	}
}

namespace ImplicitFunctions {
	constexpr auto Cube = [](auto&& EyePoint, auto&& RayDirection) {
		return ImplementationDetail::DetermineNearestIntersection(
			std::tuple{ Solvers::YZPlane(EyePoint, RayDirection, -0.5, StandardConstraints::BoundedPlane), glm::vec3{ -1, 0, 0 } },
			std::tuple{ Solvers::YZPlane(EyePoint, RayDirection, 0.5, StandardConstraints::BoundedPlane), glm::vec3{ 1, 0, 0 } },
			std::tuple{ Solvers::XZPlane(EyePoint, RayDirection, 0.5, StandardConstraints::BoundedPlane), glm::vec3{ 0, 1, 0 } },
			std::tuple{ Solvers::XZPlane(EyePoint, RayDirection, -0.5, StandardConstraints::BoundedPlane), glm::vec3{ 0, -1, 0 } },
			std::tuple{ Solvers::XYPlane(EyePoint, RayDirection, 0.5, StandardConstraints::BoundedPlane), glm::vec3{ 0, 0, 1 } },
			std::tuple{ Solvers::XYPlane(EyePoint, RayDirection, -0.5, StandardConstraints::BoundedPlane), glm::vec3{ 0, 0, -1 } }
		);
	};
	constexpr auto Sphere = [](auto&& EyePoint, auto&& RayDirection) {
		auto a = RayDirection.x * RayDirection.x + RayDirection.y * RayDirection.y + RayDirection.z * RayDirection.z;
		auto b = 2 * (RayDirection.x * EyePoint.x + RayDirection.y * EyePoint.y + RayDirection.z * EyePoint.z);
		auto c = EyePoint.x * EyePoint.x + EyePoint.y * EyePoint.y + EyePoint.z * EyePoint.z - 0.25;
		if (auto [t, IntersectionPosition] = Solvers::Quadratic(EyePoint, RayDirection, a, b, c, [](auto...) { return true; }); t != Ray::NoIntersection)
			return std::tuple{ t, glm::normalize(IntersectionPosition) };
		return std::tuple{ Ray::NoIntersection, glm::vec3{} };
	};
	constexpr auto Cylinder = [](auto&& EyePoint, auto&& RayDirection) {
		auto a = RayDirection.x * RayDirection.x + RayDirection.z * RayDirection.z;
		auto b = 2 * (RayDirection.x * EyePoint.x + RayDirection.z * EyePoint.z);
		auto c = EyePoint.x * EyePoint.x + EyePoint.z * EyePoint.z - 0.25;
		auto [t, SideIntersection] = Solvers::Quadratic(EyePoint, RayDirection, a, b, c, StandardConstraints::BoundedHeight);
		return ImplementationDetail::DetermineNearestIntersection(
			std::tuple{ Solvers::XZPlane(EyePoint, RayDirection, 0.5, StandardConstraints::CircularPlane), glm::vec3{ 0, 1, 0 } },
			std::tuple{ Solvers::XZPlane(EyePoint, RayDirection, -0.5, StandardConstraints::CircularPlane), glm::vec3{ 0, -1, 0 } },
			std::tuple{ t, glm::normalize(glm::vec3{ SideIntersection.x, 0, SideIntersection.z }) }
		);
	};
	constexpr auto Cone = [](auto&& EyePoint, auto&& RayDirection) {
		auto a = RayDirection.x * RayDirection.x + RayDirection.z * RayDirection.z - 0.25 * RayDirection.y * RayDirection.y;
		auto b = 2 * (RayDirection.x * EyePoint.x + RayDirection.z * EyePoint.z) - 0.5 * RayDirection.y * EyePoint.y + 0.25 * RayDirection.y;
		auto c = EyePoint.x * EyePoint.x + EyePoint.z * EyePoint.z - 0.25 * EyePoint.y * EyePoint.y + 0.25 * EyePoint.y - 0.0625;
		auto [t, SideIntersection] = Solvers::Quadratic(EyePoint, RayDirection, a, b, c, StandardConstraints::BoundedHeight);
		return ImplementationDetail::DetermineNearestIntersection(
			std::tuple{ Solvers::XZPlane(EyePoint, RayDirection, -0.5, StandardConstraints::CircularPlane), glm::vec3{ 0, -1, 0 } },
			std::tuple{ t, glm::normalize(glm::vec3{ 2 * SideIntersection.x, 0.25 - 0.5 * SideIntersection.y, 2 * SideIntersection.z }) }
		);
	};
}
