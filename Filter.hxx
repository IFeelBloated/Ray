#pragma once
#include "Frame.hxx"

namespace Filter {
	auto operator*(auto&& Kernel, auto&& SourceFrame) requires requires { { Kernel(SourceFrame[0].View(0, 0)) }->std::convertible_to<std::decay_t<decltype(SourceFrame[0][0][0])>>; } {
		auto ProcessedFrame = Frame{ SourceFrame[0].Height, SourceFrame[0].Width, SourceFrame.PlaneCount };
		for (auto c : Range{ SourceFrame.PlaneCount })
			for (auto y : Range{ SourceFrame[c].Height })
				for (auto x : Range{ SourceFrame[c].Width })
					ProcessedFrame[c][y][x] = Kernel(SourceFrame[c].View(y, x));
		return ProcessedFrame.Finalize();
	}
}