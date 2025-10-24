//============================================================================================================
//
//
//                  Copyright (c) 2022, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <span>
#include <vector>
#include "material/specializationConstantsLayout.hpp"

// Forward declarations
class Dx12;

/// Template specialization for specialization constants, not supported by dx12 (so this is just stub)
/// @ingroup Material
template<>
class SpecializationConstantsLayout<Dx12>
{
    SpecializationConstantsLayout( const SpecializationConstantsLayout<Dx12>& ) = delete;
    SpecializationConstantsLayout& operator=( const SpecializationConstantsLayout<Dx12>& ) = delete;
public:
    explicit SpecializationConstantsLayout( const std::span<const SpecializationConstantDescription> ) noexcept {};
    SpecializationConstantsLayout(SpecializationConstantsLayout<Dx12> && other) noexcept = default;
    SpecializationConstantsLayout& operator=( SpecializationConstantsLayout<Dx12>&& ) noexcept = delete;

private:
};

