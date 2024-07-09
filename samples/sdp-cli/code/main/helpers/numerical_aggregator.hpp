//============================================================================================================
//
//
//                  Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

#include <cstdint>
#include <vector>
#include <limits>
#include <span>
#include <cassert>
#include <cmath>

enum NumericalAggregatorValueType
{
    NUMERICAL_AGGREGATOR_CURRENT,
    NUMERICAL_AGGREGATOR_AVERAGE,
    NUMERICAL_AGGREGATOR_MIN,
    NUMERICAL_AGGREGATOR_MAX,
};

template <typename _Ty>
class NumericalAggregator
{
public:
    NumericalAggregator() = default;
    ~NumericalAggregator() = default;

    void AddEntry(_Ty entry, bool drop_zero = false) 
    {
        if(drop_zero && entry == _Ty(0)) return;

        m_data.push_back(entry);
        m_sum += entry;
        m_min_val = std::min(m_min_val, entry);
        m_max_val = std::max(m_max_val, entry);
    }

    NumericalAggregator& operator=(const _Ty& entry)
    {
        AddEntry(entry);
        Reset(true);
        return *this;
    }

    _Ty GetValue(NumericalAggregatorValueType value_type) const
    {
        switch (value_type)
        {
        case NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_CURRENT: return GetValue();
        case NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_AVERAGE: return GetAverage();
        case NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_MIN:     return GetMin();
        case NumericalAggregatorValueType::NUMERICAL_AGGREGATOR_MAX:     return GetMax();
        default:                                                         return _Ty(0);
        }
    }

    // Get the last value added
    _Ty GetValue() const
    {
        if (m_data.empty()) 
        {
            return _Ty(0);
        }

        return m_data.back();
    }

    // Get the average of all entries
    _Ty GetAverage() const 
    {
        return static_cast<_Ty>(static_cast<double>(m_sum) / std::max(_Ty(1), _Ty(m_data.size())));
    }

    // Get the lowest value
    _Ty GetMin() const 
    {
        return m_min_val;
    }

    // Get the maximum value
    _Ty GetMax() const 
    {
        return m_max_val;
    }

    std::span<const _Ty> GetStoredValues()
    {
        return m_data;
    }
    
    std::size_t GetStoredValueCount() const
    {
        return m_data.size();
    }
    
    void MatchSize(std::size_t size_to_match)
    {
        const auto old_size = m_data.size();
        m_data.resize(size_to_match);
        if (old_size < size_to_match)
        {
            std::memset(&m_data[old_size], 0, sizeof(_Ty) * (m_data.size() - old_size));
        }
    }

    void MatchSamplingFrequency(int previous_sampling_period_ms, int current_sampling_period_ms)
    {
        assert(previous_sampling_period_ms != 0 && previous_sampling_period_ms != 0);
        if (previous_sampling_period_ms == current_sampling_period_ms)
        {
            return;
        }

        const double entry_ratio = static_cast<double>(previous_sampling_period_ms) / current_sampling_period_ms;
        const std::size_t new_size = m_data.size() * entry_ratio;

        std::vector<_Ty> new_data(new_size);

#if 0
        if (previous_sampling_period_ms > current_sampling_period_ms)
        {
            for (int i=0; i<m_data.size(); i++)
            {
                for (int j = 0; j < std::floor(entry_ratio); j++)
                {
                    const int new_data_index = i * std::floor(entry_ratio) + j;
                    new_data[new_data_index] = m_data[i];
                }
            }
        }
        else 
#endif
        {
            for (int i=0; i<new_data.size(); i++)
            {
                const int old_data_index = i * (1.0 / entry_ratio);
                new_data[i] = m_data[old_data_index];
            }        
        }

        m_data = std::move(new_data);
    }

    void Reset(bool keep_last = false)
    { 
        if (keep_last && !m_data.empty())
        {
            m_data.erase(m_data.begin(), m_data.end() - 1);
            m_sum     = m_data[0];
            m_min_val = m_data[0];
            m_max_val = m_data[0];
        }
        else
        {
            m_data.clear();
            m_sum     = 0;
            m_min_val = std::numeric_limits<_Ty>::max();
            m_max_val = std::numeric_limits<_Ty>::min();
        }

    }

private:
    
    std::vector<_Ty> m_data;
    _Ty              m_sum = _Ty(0);
    _Ty              m_min_val = std::numeric_limits<_Ty>::max();
    _Ty              m_max_val = std::numeric_limits<_Ty>::min();
};
