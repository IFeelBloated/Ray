#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <tuple>
#include <array>
#include <list>
#include <unordered_map>
#include <type_traits>
#include <functional>
#include <algorithm>
#include <numeric>
#include <execution>
#include <numbers>
#include <concepts>
#include <limits>
#include <utility>
#include <memory>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstdlib>

#define field(FieldIdentifier, ...) std::decay_t<decltype(__VA_ARGS__)> FieldIdentifier = __VA_ARGS__
#define Forward(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

using namespace std::literals;

constexpr auto operator""_u64(unsigned long long Value) {
	return static_cast<std::uint64_t>(Value);
}

constexpr auto operator""_i64(unsigned long long Value) {
	return static_cast<std::int64_t>(Value);
}

constexpr auto operator""_u32(unsigned long long Value) {
	return static_cast<std::uint32_t>(Value);
}

constexpr auto operator""_i32(unsigned long long Value) {
	return static_cast<std::int32_t>(Value);
}

constexpr auto operator""_u16(unsigned long long Value) {
	return static_cast<std::uint16_t>(Value);
}

constexpr auto operator""_i16(unsigned long long Value) {
	return static_cast<std::int16_t>(Value);
}

constexpr auto operator""_u8(unsigned long long Value) {
	return static_cast<std::uint8_t>(Value);
}

constexpr auto operator""_i8(unsigned long long Value) {
	return static_cast<std::int8_t>(Value);
}

constexpr auto operator""_z(unsigned long long Value) {
	return static_cast<std::ptrdiff_t>(Value);
}

constexpr auto operator""_uz(unsigned long long Value) {
	return static_cast<std::size_t>(Value);
}

constexpr auto Bottom = [](auto&&) {};

struct Range {
	field(Startpoint, 0_z);
	field(Endpoint, 0_z);
	field(Step, 1_z);

private:
	struct Iterator {
		field(Cursor, 0_z);
		field(Step, 0_z);

	public:
		constexpr auto operator*() const {
			return Cursor;
		}
		constexpr auto& operator++() {
			Cursor += Step;
			return *this;
		}
		constexpr auto operator!=(auto&& OtherIterator) const {
			if (Step > 0)
				return Cursor < OtherIterator.Cursor;
			else
				return Cursor > OtherIterator.Cursor;
		}
	};

public:
	constexpr Range() = default;
	constexpr Range(std::integral auto Endpoint) {
		if (Endpoint < 0)
			this->Step = -1;
		this->Endpoint = Endpoint;
	}
	constexpr Range(std::integral auto Startpoint, std::integral auto Endpoint) {
		if (Startpoint > Endpoint)
			this->Step = -1;
		this->Startpoint = Startpoint;
		this->Endpoint = Endpoint;
	}
	constexpr Range(std::integral auto Startpoint, std::integral auto Endpoint, std::integral auto Step) {
		this->Startpoint = Startpoint;
		this->Endpoint = Endpoint;
		this->Step = Step;
	}

public:
	constexpr auto begin() const {
		return Iterator{ .Cursor = Startpoint, .Step = Step };
	}
	constexpr auto end() const {
		return Iterator{ .Cursor = Endpoint, .Step = Step };
	}
};

template<typename UnknownType, typename ReferenceType>
concept SubtypeOf = std::same_as<std::decay_t<UnknownType>, ReferenceType> || std::derived_from<std::decay_t<UnknownType>, ReferenceType>;

template<typename UnknownType, typename ...ReferenceTypes>
concept AnyOf = (SubtypeOf<UnknownType, ReferenceTypes> || ...);

template<typename UnknownType, typename ...ReferenceTypes>
concept AnyBut = !AnyOf<UnknownType, ReferenceTypes...>;

template<typename UnknownType, typename ReferenceType>
concept ExplicitlyConvertibleTo = requires(UnknownType x) { static_cast<ReferenceType>(Forward(x)); };

template<typename UnknownType, typename ReferenceType>
concept ConstructibleFrom = ExplicitlyConvertibleTo<ReferenceType, std::decay_t<UnknownType>>;

template<typename UnknownType>
concept BuiltinArray = std::is_array_v<std::remove_cvref_t<UnknownType>>;

template<typename UnknownType>
concept Advanceable = requires(UnknownType x) { ++x; };

template<typename UnknownType>
concept Iterable = BuiltinArray<UnknownType> || requires(UnknownType x) {
	{ x.begin() }->Advanceable;
	{ *x.begin() }->AnyBut<void>;
	{ x.begin() != x.end() }->ExplicitlyConvertibleTo<bool>;
};

template<typename UnknownType>
concept Countable = std::integral<std::decay_t<UnknownType>> || std::is_enum_v<std::decay_t<UnknownType>>;

template<typename UnknownType>
concept Real = std::integral<std::decay_t<UnknownType>> || std::floating_point<std::decay_t<UnknownType>>;

template<typename UnknownType, typename FunctionType, auto Index>
concept ApplicableAt = requires(UnknownType x, std::decay_t<FunctionType> Transformation) { Transformation(std::get<Index>(x)); };

template<typename UnknownType, typename FunctionType>
concept ElementwiseApplicableTo = []<auto ...x>(std::index_sequence<x...>) { return (ApplicableAt<UnknownType, FunctionType, x> && ...); }(std::make_index_sequence<std::tuple_size<std::remove_cvref_t<UnknownType>>{}>{});

template<typename UnknownType>
concept Expandable = ElementwiseApplicableTo<UnknownType, decltype(Bottom)>;

auto& operator+=(Iterable auto&& Self, Iterable auto&& Items) requires requires {
	Self.insert(Self.end(), Items.begin(), Items.end());
	{ *Items.begin() }->SubtypeOf<std::decay_t<decltype(*Self.begin())>>;
} {
	Self.insert(Self.end(), Items.begin(), Items.end());
	return Self;
}

auto& operator+=(Iterable auto&& Self, Iterable auto&& Items) requires requires {
	Self.insert(Items.begin(), Items.end());
	{ *Items.begin() }->SubtypeOf<std::decay_t<decltype(*Self.begin())>>;
} {
	Self.insert(Items.begin(), Items.end());
	return Self;
}

auto& operator+=(Iterable auto&& Self, auto&& Item) requires requires {
	Self.insert(Self.end(), Forward(Item));
	requires (requires { Self.insert(Forward(Item)); } == false);
} {
	Self.insert(Self.end(), Forward(Item));
	return Self;
}

auto& operator+=(Iterable auto&& Self, auto&& Item) requires requires { Self.insert(Forward(Item)); } {
	Self.insert(Forward(Item));
	return Self;
}

auto operator+(Iterable auto&& PrimaryContainer, Iterable auto&& Items) requires requires { PrimaryContainer.insert(PrimaryContainer.end(), Items.begin(), Items.end()); } {
	auto PrimaryContainerReplica = Forward(PrimaryContainer);
	PrimaryContainerReplica.insert(PrimaryContainerReplica.end(), Items.begin(), Items.end());
	return PrimaryContainerReplica;
}

auto operator+(auto&& LeftHandSideOperand, auto&& RightHandSideOperand) requires requires { std::tuple_cat(Forward(LeftHandSideOperand), Forward(RightHandSideOperand)); } {
	return std::tuple_cat(Forward(LeftHandSideOperand), Forward(RightHandSideOperand));
}

auto& operator*=(Iterable auto&& Self, std::integral auto Multiplier) requires requires { Self += Self; } {
	if (Multiplier > 0)
		for (auto Replica = Self; auto _ : Range{ Multiplier - 1 })
			Self += Replica;
	else
		if constexpr (requires { Self.clear(); })
			Self.clear();
		else
			Self = std::decay_t<decltype(Self)>{};
	return Self;
}

auto operator*(std::integral auto Multiplier, Iterable auto&& Container) requires requires { Container *= Multiplier; } {
	auto ContainerReplica = Forward(Container);
	ContainerReplica *= Multiplier;
	return ContainerReplica;
}

auto operator*(Iterable auto&& Container, std::integral auto Multiplier) requires requires { Container *= Multiplier; } {
	return Multiplier * Forward(Container);
}

namespace Arithmetic {
	auto Max(auto x, auto y) {
		return x > y ? x : y;
	}
	auto Min(auto x, auto y) {
		return x < y ? x : y;
	}
}

namespace Reflection::Private {
	template<typename, typename...>
	struct ContainerReplaceTypeArgument {};
	template<template<typename...> typename ContainerTypeConstructor, typename ...TypesBeingReplaced, typename ...TargetElementTypes>
	struct ContainerReplaceTypeArgument<ContainerTypeConstructor<TypesBeingReplaced...>, TargetElementTypes...> {
		using ReassembledType = ContainerTypeConstructor<TargetElementTypes...>;
	};
	template<template<typename, auto> typename ContainerTypeConstructor, typename TypeBeingReplaced, auto Length, typename TargetElementType>
	struct ContainerReplaceTypeArgument<ContainerTypeConstructor<TypeBeingReplaced, Length>, TargetElementType> {
		using ReassembledType = ContainerTypeConstructor<TargetElementType, Length>;
	};
	template<template<typename, typename> typename ContainerTypeConstructor, template<typename> typename AllocatorTypeConstructor, typename TypeBeingReplaced, typename TargetElementType>
	struct ContainerReplaceTypeArgument<ContainerTypeConstructor<TypeBeingReplaced, AllocatorTypeConstructor<TypeBeingReplaced>>, TargetElementType> {
		using ReassembledType = ContainerTypeConstructor<TargetElementType, AllocatorTypeConstructor<TargetElementType>>;
	};

	template<typename UnknownType>
	struct ExtractInnermostTypeArgument {
		using IrreducibleType = UnknownType;
	};
	template<Iterable UnknownType> requires (BuiltinArray<UnknownType> == false)
	struct ExtractInnermostTypeArgument<UnknownType> {
		using InnerType = std::remove_cvref_t<decltype(*std::declval<UnknownType>().begin())>;
		using IrreducibleType = ExtractInnermostTypeArgument<InnerType>::IrreducibleType;
	};
	template<typename UnknownType, auto Length>
	struct ExtractInnermostTypeArgument<UnknownType[Length]> {
		using InnerType = std::remove_cvref_t<UnknownType>;
		using IrreducibleType = ExtractInnermostTypeArgument<InnerType>::IrreducibleType;
	};
}

namespace Reflection {
	template<typename ReferenceContainerType, typename ...TargetElementTypes>
	using ContainerReplaceElementType = Private::ContainerReplaceTypeArgument<std::decay_t<ReferenceContainerType>, TargetElementTypes...>::ReassembledType;

	template<typename UnknownType>
	using ExtractInnermostElementType = Private::ExtractInnermostTypeArgument<std::remove_cvref_t<UnknownType>>::IrreducibleType;
}

namespace ContainerManipulators {
	auto Distance(auto&& Startpoint, auto&& Endpoint) {
		if constexpr (requires { { Endpoint - Startpoint }->ExplicitlyConvertibleTo<std::ptrdiff_t>; })
			return static_cast<std::ptrdiff_t>(Endpoint - Startpoint);
		else {
			auto DistanceCounter = 0_z;
			for (auto Cursor = Forward(Startpoint); Cursor != Endpoint; ++Cursor)
				++DistanceCounter;
			return DistanceCounter;
		}
	}
	template<typename TargetContainerType>
	auto MapWithNaturalTransformation(auto&& TransformationForEachElement, auto&& SourceContainer) {
		constexpr auto SourceIsMovable = std::is_rvalue_reference_v<decltype(SourceContainer)>;
		auto TargetContainer = TargetContainerType{};
		auto ApplyElementWiseTransformation = [&](auto&& Value) {
			if constexpr (SourceIsMovable)
				return TransformationForEachElement(std::move(Value));
			else
				return TransformationForEachElement(Value);
		};
		auto ConstructPlaceholderForTransformedElement = [&] {
			if constexpr (requires { { SourceContainer }->BuiltinArray; })
				return ApplyElementWiseTransformation(SourceContainer[0]);
			else
				return ApplyElementWiseTransformation(*SourceContainer.begin());
		};
		auto EstimateSourceContainerSize = [&] {
			if constexpr (requires { { SourceContainer }->BuiltinArray; })
				return sizeof(SourceContainer) / sizeof(SourceContainer[0]);
			else if constexpr (requires { { SourceContainer.size() }->std::integral; })
				return SourceContainer.size();
			else
				return Distance(SourceContainer.begin(), SourceContainer.end());
		};
		auto CacheTransformedElements = [&] {
			using TransformedElementType = std::decay_t<decltype(ConstructPlaceholderForTransformedElement())>;
			auto CachedElements = std::vector<TransformedElementType>{};
			CachedElements.reserve(EstimateSourceContainerSize());
			for (auto&& x : SourceContainer)
				CachedElements.push_back(ApplyElementWiseTransformation(x));
			return CachedElements;
		};
		auto ExtractCursors = [&] {
			if constexpr (requires { { SourceContainer }->BuiltinArray; })
				return std::tuple{ TargetContainer.begin(), SourceContainer };
			else
				return std::tuple{ TargetContainer.begin(), SourceContainer.begin() };
		};
		if constexpr (requires { TargetContainer.reserve(EstimateSourceContainerSize()); })
			TargetContainer.reserve(EstimateSourceContainerSize());
		if constexpr (requires { TargetContainer.push_back(ConstructPlaceholderForTransformedElement()); })
			for (auto&& x : SourceContainer)
				TargetContainer.push_back(ApplyElementWiseTransformation(x));
		else if constexpr (requires { TargetContainer.insert(ConstructPlaceholderForTransformedElement()); })
			for (auto&& x : SourceContainer)
				TargetContainer.insert(ApplyElementWiseTransformation(x));
		else if constexpr (requires { TargetContainer.push_front(ConstructPlaceholderForTransformedElement()); })
			for (auto CachedElements = CacheTransformedElements(); CachedElements.empty() == false; CachedElements.pop_back())
				TargetContainer.push_front(std::move(CachedElements.back()));
		else
			for (auto [TargetContainerCursor, SourceContainerCursor] = ExtractCursors(); auto _ : Range{ Arithmetic::Min(EstimateSourceContainerSize(), Distance(TargetContainer.begin(), TargetContainer.end())) }) {
				*TargetContainerCursor = ApplyElementWiseTransformation(*SourceContainerCursor);
				++TargetContainerCursor;
				++SourceContainerCursor;
			}
		return TargetContainer;
	}
	auto fMap(auto&& TransformationForEachElement, auto&& SourceContainer) {
		using TransformedElementType = decltype(TransformationForEachElement(*SourceContainer.begin()));
		using TargetContainerType = Reflection::ContainerReplaceElementType<decltype(SourceContainer), TransformedElementType>;
		return MapWithNaturalTransformation<TargetContainerType>(Forward(TransformationForEachElement), Forward(SourceContainer));
	}
}

auto& operator<<(SubtypeOf<std::ostream> auto&, const Expandable auto& Container) requires (requires { { Container }->Iterable; } == false);

auto& operator<<(SubtypeOf<std::ostream> auto& Printer, const Iterable auto& Container) requires requires {
	Printer << std::declval<Reflection::ExtractInnermostElementType<decltype(Container)>>();
} {
	auto [Startpoint, Endpoint] = [&] {
		if constexpr (requires { { Container }->BuiltinArray; })
			return std::tuple{ Container, Container + sizeof(Container) / sizeof(Container[0]) };
		else
			return std::tuple{ Container.begin(), Container.end() };
	}();
	Printer << "[";
	for (auto Cursor = Startpoint; Cursor != Endpoint; ++Cursor)
		if (Cursor != Startpoint)
			Printer << ", " << *Cursor;
		else
			Printer << *Cursor;
	Printer << "]";
	return Printer;
}

auto& operator<<(SubtypeOf<std::ostream> auto& Printer, const Expandable auto& Container) requires (requires { { Container }->Iterable; } == false) {
	Printer << "(";
	if constexpr (std::tuple_size_v<std::decay_t<decltype(Container)>> != 0)
		std::apply([&](auto&& FirstElement, auto&& ...Elements) {
			Printer << FirstElement;
			([&](auto&& x) { Printer << ", " << x; }(Elements), ...);
		}, Container);
	Printer << ")";
	return Printer;
}

auto operator|(Iterable auto&& SourceContainer, auto&& TransformationForEachElement) requires requires {
	{ TransformationForEachElement(*SourceContainer.begin()) }->AnyBut<void>;
} {
	return ContainerManipulators::fMap(Forward(TransformationForEachElement), Forward(SourceContainer));
}

decltype(auto) operator|(Iterable auto&& SourceContainer, auto&& TransformationForEachElement) requires requires {
	{ TransformationForEachElement(*SourceContainer.begin()) }->SubtypeOf<void>;
} {
	using ContainerType = std::remove_cvref_t<decltype(SourceContainer)>;
	for (auto&& x : SourceContainer)
		TransformationForEachElement(x);
	if constexpr (std::is_rvalue_reference_v<decltype(SourceContainer)>)
		return ContainerType{ std::move(SourceContainer) };
	else
		return SourceContainer;
}

auto operator|(auto&& SourceContainer, auto&& TransformationForEachElement) requires requires {
	{ SourceContainer }->ElementwiseApplicableTo<decltype(TransformationForEachElement)>;
	requires (requires { { SourceContainer }->Iterable; } == false);
} {
	auto ApplyToEachElement = [&](auto&& x) {
		if constexpr (requires { { TransformationForEachElement(x) }->AnyBut<void>; })
			return TransformationForEachElement(Forward(x));
		else {
			TransformationForEachElement(x);
			return Forward(x);
		}
	};
	using ContainerType = std::remove_cvref_t<decltype([]<auto ...x>(std::index_sequence<x...>)->auto& {
		return *static_cast<Reflection::ContainerReplaceElementType<decltype(SourceContainer), decltype(ApplyToEachElement(std::get<x>(SourceContainer)))...>*>(nullptr);
	}(std::make_index_sequence<std::tuple_size_v<std::remove_cvref_t<decltype(SourceContainer)>>>{}))>;
	return std::apply([&](auto&& ...x) { return ContainerType{ ApplyToEachElement(Forward(x))... }; }, Forward(SourceContainer));
}