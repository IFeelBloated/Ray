#pragma once
#include "Infrastructure.hxx"

namespace RemappingFunctions {
	constexpr auto Replicate = [](auto x, auto Bound) {
		x = Arithmetic::Min(Arithmetic::Max(x, 0), Bound - 1);
		return x;
	};
	constexpr auto Reflect = [](auto x, auto Bound) {
		while (x < 0 || x >= Bound) {
			x = std::abs(x);
			x -= Bound - 1;
			x = -std::abs(x);
			x += Bound - 1;
		}
		return x;
	};
}

namespace Filter::ImplementationDetail {
	template<typename PixelType>
	struct CanvasProxy {
		field(Height, 0_uz);
		field(Width, 0_uz);
		field(Stride, 0_z);
		field(Data, static_cast<PixelType*>(nullptr));

	public:
		auto operator[](auto y) const {
			return Data + y * Stride;
		}
	};

	template<typename PixelType>
	struct Plane : CanvasProxy<PixelType> {
	private:
		using EmptyType = struct {};
		struct ExistentialTypeForRemappingFunction {
			using QuantificationBound = auto(std::ptrdiff_t, std::ptrdiff_t, std::size_t, std::size_t)->std::array<std::ptrdiff_t, 2>;

		public:
			field(HostedFunction, std::function<QuantificationBound>{});

		public:
			auto& operator=(AnyBut<ExistentialTypeForRemappingFunction> auto&& RemappingFunction) {
				if constexpr (requires { { RemappingFunction(0_z, 0_z, 0_uz, 0_uz) }->std::same_as<std::array<std::ptrdiff_t, 2>>; })
					HostedFunction = Forward(RemappingFunction);
				else
					HostedFunction = [RemappingFunction = Forward(RemappingFunction)](auto y, auto x, auto Height, auto Width) {
					if constexpr (requires { { RemappingFunction(y, Height) }->std::same_as<std::ptrdiff_t>; })
						return std::array{ RemappingFunction(y, Height), RemappingFunction(x, Width) };
					else
						static_assert(sizeof(RemappingFunction) == -1, "Plane: specified remapping function has an incompatible signature.");
				};
				return *this;
			}
			auto operator()(auto y, auto x, auto Height, auto Width) const {
				return HostedFunction(y, x, Height, Width);
			}
		};

	public:
		static constexpr auto Readonly = std::is_const_v<PixelType>;
		static inline auto DefaultRemappingFunction = [] {
			if constexpr (auto PolymorphicRemappingFunction = ExistentialTypeForRemappingFunction{}; Readonly)
				return PolymorphicRemappingFunction = RemappingFunctions::Reflect;
			else
				return EmptyType{};
		}();

	public:
		[[no_unique_address]]
		field(OutOfBoundsRemapping, DefaultRemappingFunction);

	private:
		struct RemappedAccess {
			field(TargetPlane, static_cast<const Plane*>(nullptr));
			field(yAbsolute, 0_z);
			field(xOffset, 0_z);

		public:
			auto operator[](std::integral auto x) const {
				if (auto xAbsolute = x + xOffset; xAbsolute < 0 || yAbsolute < 0 || xAbsolute >= TargetPlane->Width || yAbsolute >= TargetPlane->Height) {
					auto [yRemapped, xRemapped] = TargetPlane->OutOfBoundsRemapping(yAbsolute, xAbsolute, TargetPlane->Height, TargetPlane->Width);
					return TargetPlane->DirectAccess()[yRemapped][xRemapped];
				}
				else [[likely]]
					return TargetPlane->DirectAccess()[yAbsolute][xAbsolute];
			}
		};

	private:
		struct OffsetView {
			field(TargetPlane, static_cast<const Plane*>(nullptr));
			field(yOffset, 0_z);
			field(xOffset, 0_z);

		public:
			auto operator[](std::integral auto y) const {
				return RemappedAccess{ .TargetPlane = TargetPlane, .yAbsolute = y + yOffset, .xOffset = xOffset };
			}
			auto QueryCoordinates() const {
				return std::array{ yOffset, xOffset };
			}
			auto View(std::integral auto y, std::integral auto x) const {
				return OffsetView{ .TargetPlane = TargetPlane, .yOffset = yOffset + y, .xOffset = xOffset + x };
			}
		};

	public:
		auto operator[](std::integral auto y) const {
			if constexpr (Readonly)
				return RemappedAccess{ .TargetPlane = this, .yAbsolute = y };
			else
				return CanvasProxy<PixelType>::operator[](y);
		}
		auto View(std::integral auto y, std::integral auto x) const requires Readonly {
			return OffsetView{.TargetPlane = this, .yOffset = y, .xOffset = x };
		}
		auto& DirectAccess() const requires Readonly {
			return static_cast<const CanvasProxy<PixelType>&>(*this);
		}
	};
}

namespace Filter {
	template<typename PixelType = double>
	struct Frame {
	private:
		using PlaneType = ImplementationDetail::Plane<PixelType>;

	public:
		static constexpr auto Readonly = std::is_const_v<PixelType>;

	public:
		field(PlaneCount, 0_uz);
		field(Planes, std::array{ PlaneType{}, PlaneType{}, PlaneType{} });
		field(Storage, std::vector<std::decay_t<PixelType>>{});

	public:
		Frame() = default;
		Frame(std::integral auto Height, std::integral auto Width, Countable auto PlaneCount) {
			this->PlaneCount = PlaneCount;
			this->Storage.resize(static_cast<std::size_t>(PlaneCount) * Height * Width);
			this->RefreshPlanes(Height, Width);
		}

	public:
		auto& operator[](Countable auto x) {
			return Planes[static_cast<std::size_t>(x)];
		}
		auto& operator[](Countable auto x) const {
			return Planes[static_cast<std::size_t>(x)];
		}

	public:
		auto RefreshPlanes(std::integral auto Height, std::integral auto Width) {
			for (auto Index : Range{ this->PlaneCount })
				Planes[Index] = PlaneType{ Height, Width, Width, Storage.data() + Index * Height * Width };
		}
		auto Finalize() requires (Readonly == false) {
			auto FinalizedFrame = Frame<const std::decay_t<PixelType>>{};
			FinalizedFrame.PlaneCount = this->PlaneCount;
			std::swap(FinalizedFrame.Storage, this->Storage);
			FinalizedFrame.RefreshPlanes(this->Planes[0].Height, this->Planes[0].Width);
			return FinalizedFrame;
		}
	};
}