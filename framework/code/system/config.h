//============================================================================================================
//
//
//                  Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
//                              SPDX-License-Identifier: BSD-3-Clause
//
//============================================================================================================
#pragma once

///
/// @defgroup Config
/// @brief Config file serializer/deserializer.
/// @details Allows registered variables to be loaded/saved to text configuration files.
/// 
/// To add unsupported types implement the appropriate ReadFromText and WriteToText templates.
/// 

/// @addtogroup Config
/// @{

#ifndef _SYS_CONFIG_H_
#define _SYS_CONFIG_H_

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "containers.h"

/// @ingroup Config
/// Parse the supplied commandline and fill any registered variables with the appropriate values.
void LoadCommandLineVariables(char* argv[], int argc);
/// Load and parse the supplied text file and fill any registered variables with the appropriate values.
void LoadVariableFile(const char* filename);
/// Parse the supplied text and fill any registered variables with the appropriate values.
void LoadVariableBuffer(const char* buffer);
/// Write the current registered variable values to the supplied filename.
void WriteVariableFile(const char* filename);
/// Parse the supplied text and fill the (single) registered variable with the appropriate value.
void LoadVariable(const char* text);

namespace {

inline bool CompareTextCaseless(const char* s1, const char* s2, size_t MaxStringLength = SIZE_MAX )
{
	for (int a = 0; a < MaxStringLength; a++) {
		unsigned long x = *reinterpret_cast<const unsigned char*>(s1 + a);
		unsigned long y = *reinterpret_cast<const unsigned char*>(s2 + a);
		if (x - 65 < 26UL) x += 32;
		if (y - 65 < 26UL) y += 32;
		if (x != y) return false;
		if (x == 0) break;
	}

	return true;
}

inline bool CompareTextLessThan(const char* s1, const char* s2, size_t MaxStringLength = SIZE_MAX)
{
	for (int a = 0; a < MaxStringLength; a++) {
		unsigned long x = *reinterpret_cast<const unsigned char*>(s1 + a);
		unsigned long y = *reinterpret_cast<const unsigned char*>(s2 + a);
		if (x - 'a' < 26UL) x -= 32;
		if (y - 'a' < 26UL) y -= 32;
		if ((x != y) || (x == 0)) return (x < y);
	}
	return false;
}

inline bool CompareTextLessEqual(const char* s1, const char* s2, size_t MaxStringLength = SIZE_MAX )
{
	for (int a = 0; a < MaxStringLength; a++) {
		unsigned long x = *reinterpret_cast<const unsigned char*>(s1 + a);
		unsigned long y = *reinterpret_cast<const unsigned char*>(s2 + a);
		if (x - 'a' < 26UL) x -= 32;
		if (y - 'a' < 26UL) y -= 32;
		if ((x != y) || (x == 0)) return (x <= y);
	}
	return true;
}

inline long StringToInteger(const char* text)
{
	long value = 0;
	bool negative = false;

	for (;;) {
		unsigned long x = *text++;
		if (x == 0) break;

		if (x == '-') {
			negative = true;
		}
		else {
			x -= 48;
			if (x < 10) value = value * 10 + x;
		}
	}

	if (negative) value = -value;
	return value;
}

inline float StringToFloat(const char* text)
{
	float value = 0.0F;
	float expon = 0.0F;
	float decplace = 0.1F;

	bool negative = false;
	bool exponent = false;
	bool exponNeg = false;
	bool decimal = false;

	for (;;) {
		unsigned long x = *text++;
		if (x == 0) break;

		if (x == '-') {
			if (exponent) exponNeg = true;
			else negative = true;
		}
		else if (x == '.') {
			decimal = true;
		}
		else if ((x == 'e') || (x == 'E')) {
			exponent = true;
		}
		else {
			x -= 48;
			if (x < 10) {
				if (exponent) {
					expon = expon * 10.0F + x;
				}
				else {
					if (decimal) {
						value += x * decplace;
						decplace *= 0.1F;
					}
					else {
						value = value * 10.0F + x;
					}
				}
			}
		}
	}

	if (exponent) {
		if (exponNeg) expon = -expon;
		value *= (float)pow(10.0f, expon);
	}

	if (negative) value = -value;
	return value;
}

inline bool StringToBool(const char* text)
{
	if (CompareTextCaseless(text, "yes") || CompareTextCaseless(text, "true") || CompareTextCaseless(text, "on")) {
		return true;
	}
	else
		if (CompareTextCaseless(text, "no") || CompareTextCaseless(text, "false") || CompareTextCaseless(text, "off")) {
			return false;
		}
		else {
			int i = atoi(text);
			return i != 0;
		}
}

inline long IntegerToString(long num, char* text, long max)
{
	char c[16];

	bool negative = (num < 0);
	num = abs(num) & 0x7FFFFFFF;

	int length = 0;
	do {
		long p = num % 10;
		c[length++] = (char)(p + 48);
		num /= 10;
	} while (num != 0);

	int a = -1;
	if (negative) {
		if (++a < max) {
			text[a] = '-';
		}
		else {
			text[a] = 0;
			return (a);
		}
	}

	do {
		if (++a < max) {
			text[a] = c[--length];
		}
		else {
			text[a] = 0;
			return (a);
		}
	} while (length != 0);

	text[++a] = 0;
	return (a);
}

inline long FloatToString(float num, char* text, long max)
{
	if (max < 1) {
		text[0] = 0;
		return (0);
	}

	long binary = *reinterpret_cast<long*>(&num);
	long exponent = (binary >> 23) & 0xFF;

	if (exponent == 0) {
		if (max >= 3) {
			text[0] = '0';
			text[1] = '.';
			text[2] = '0';
			text[3] = 0;
			return (3);
		}

		text[0] = '0';
		text[1] = 0;
		return (1);
	}

	long mantissa = binary & 0x007FFFFF;

	if (exponent == 0xFF) {
		if (max >= 4) {
			bool b = (binary < 0);
			if (b) *text++ = '-';

			if (mantissa == 0) {
				text[0] = 'I';
				text[1] = 'N';
				text[2] = 'F';
				text[3] = 0;
			}
			else {
				text[0] = 'N';
				text[1] = 'A';
				text[2] = 'N';
				text[3] = 0;
			}

			return (3 + b);
		}

		text[0] = 0;
		return (0);
	}

	long power = 0;
	float absolute = (float)fabs(num);
	if ((!(absolute > 1.0e-4F)) || (!(absolute < 1.0e5F))) {
		float f = (float)floor(log10(absolute));
		absolute /= (float)pow(10.0f, f);
		power = (long)f;

		binary = *reinterpret_cast<long *>(&absolute);
		exponent = (binary >> 23) & 0xFF;
		mantissa = binary & 0x007FFFFF;
	}

	exponent -= 0x7F;
	mantissa |= 0x00800000;

	int len = 0;
	if (num < 0.0F) {
		text[0] = '-';
		len = 1;
	}

	if (exponent >= 0) {
		long whole = mantissa >> (23 - exponent);
		mantissa = (mantissa << exponent) & 0x007FFFFF;

		len += IntegerToString(whole, &text[len], max - len);
		if (len < max) text[len++] = '.';
		if (len == max) goto end;
	}
	else {
		if (len + 2 <= max) {
			text[len++] = '0';
			text[len++] = '.';
			if (len == max) goto end;
		}
		else {
			if (len < max) text[len++] = '0';
			goto end;
		}

		mantissa >>= -exponent;
	}

	for (int a = 0, zeroCount = 0, nineCount = 0; (a < 7) && (len < max); a++) {
		mantissa *= 10;
		long n = (mantissa >> 23) + 48;
		text[len++] = (char)n;

		if (n == '0') {
			if ((++zeroCount >= 4) && (a >= 4)) break;
		}
		else if (n == '9') {
			if ((++nineCount >= 4) && (a >= 4)) break;
		}

		mantissa &= 0x007FFFFF;
		if (mantissa < 2) break;
	}

	if ((text[len - 1] == '9') && (text[len - 2] == '9')) {
		for (int a = len - 3;; a--) {
			char c = text[a];
			if (c != '9') {
				if (c != '.') {
					text[a] = c + 1;
					len = a + 1;
				}

				break;
			}
		}
	}
	else {
		while (text[len - 1] == '0') len--;
		if (text[len - 1] == '.') text[len++] = '0';
	}

	if ((power != 0) && (len < max)) {
		text[len++] = 'e';
		return (IntegerToString(power, &text[len], max - len));
	}

end:
	text[len] = 0;
	return (len);
}

template< size_t MaxStringLength >
struct ConstCharPtr {
private:
	const char	*ptr;
public:
	ConstCharPtr() {}

	ConstCharPtr(const char* c)
	{
		ptr = c;
	}

	operator const char*() const
	{
		return ptr;
	}

	ConstCharPtr& operator =(const char* c)
	{
		ptr = c;
		return *this;
	}

	bool operator ==(const char* c) const
	{
		return CompareTextCaseless(ptr, c, MaxStringLength);
	}

	bool operator !=(const char* c) const
	{
		return !CompareTextCaseless(ptr, c, MaxStringLength);
	}

	bool operator <(const char* c) const
	{
		return CompareTextLessThan(ptr, c, MaxStringLength);
	}

	bool operator <=(const char* c) const
	{
		return CompareTextLessEqual(ptr, c, MaxStringLength);
	}

	bool operator >(const char* c) const
	{
		return !CompareTextLessEqual(ptr, c, MaxStringLength);
	}

	bool operator >=(const char* c) const
	{
		return !CompareTextLessThan(ptr, c, MaxStringLength);
	}
};

}; // namespace

enum {
	/// Mark variable as non persistant (do not write back to config)
	kVariableNonpersistent = 1 << 0,
	/// Mark variable as persistant (write back to config)
	kVariablePermanent = 1 << 1,
	/// Mark variable as modified (has been set outside of the app default - although may have been set back to the default value!)
	kVariableModified = 1 << 2,
};

enum {
	kMaxVariableNameLength = 31,
	kMaxVariableValueLength = 255
};

class VariableBase : public MapElement<VariableBase> {
private:
	unsigned long mFlags;
	char          mName[kMaxVariableNameLength+1];
protected:
	VariableBase(const char* name, unsigned long flags = 0)
	{
		mFlags = flags;
		strncpy(mName, name, kMaxVariableNameLength);
		mName[kMaxVariableNameLength] = 0;
	}
	virtual ~VariableBase() {}
public:
	typedef ConstCharPtr<kMaxVariableNameLength> KeyType;

	unsigned long GetFlags() const
	{
		return mFlags;
	}

	void SetFlags(unsigned long flags)
	{
		mFlags = flags;
	}

	KeyType GetKey() const
	{
		return mName;
	}

	const char* GetName() const
	{
		return mName;
	}

	virtual void SetValue(const char* const text) = 0;
	virtual void GetValue(char* text, long max) const = 0;
};

template <class Type>
void ReadFromText(Type*, const char* const)
{
//	//assert(false); // this type is unsupported
}

template <>
inline void ReadFromText<char>(char* val, const char* const text)
{
	*val = *text;
}

template <>
inline void ReadFromText<char*>(char** val, const char* const text)
{
	*val = new char[strlen(text) + 1];
	strcpy(*val, text);
}

template <>
inline void ReadFromText<bool>(bool* val, const char* const text)
{
	*val = StringToBool(text);
}

template <>
inline void ReadFromText<float>(float* val, const char* const text)
{
	*val = StringToFloat(text);
}

template <>
inline void ReadFromText<double>(double* val, const char* const text)
{
	*val = StringToFloat(text);
}

template <>
inline void ReadFromText<unsigned long>(unsigned long* val, const char* const text)
{
	*val = StringToInteger(text);
}


template <>
inline void ReadFromText<long>(long* val, const char* const text)
{
	*val = StringToInteger(text);
}

template <>
inline void ReadFromText<unsigned int>(unsigned int* val, const char* const text)
{
	*val = StringToInteger(text);
}

template <>
inline void ReadFromText<int>(int* val, const char* const text)
{
	*val = StringToInteger(text);
}

template <size_t N>
inline void ReadFromText( float (*val)[N], const char* const text)
{
	if( text[0] == '{' )
	{
		char split[kMaxVariableValueLength];
		strncpy( split, text+1, sizeof(split)-1);
		split[sizeof(split) - 1] = 0;

		char delimiter = strchr( split, ',' ) != nullptr ? ',' : ' ';

		char* pValue = &split[0];

		for(int i=0; i<N; ++i)
		{
			// Skip leading spaces
			while( *pValue && *pValue == ' ' )
				++pValue;
			if( *pValue == 0 )
				break;

			// look for the delimiter (and then '}' for the final value)
			char* pDelim = pValue;
			if( i == (N - 1) )
				while( *pDelim && *pDelim != '}' )
					++pDelim;
			else
				while( *pDelim && *pDelim != delimiter )
					++pDelim;
			if( *pDelim == 0 )
				break;
			*pDelim = 0;
			// remove any spaces before the delimiter
			char* pSpace = pDelim;
			while( (--pSpace) > pValue && *pSpace == ' ' )
				*pSpace = 0;

			(*val)[i] = StringToFloat( pValue );

			pValue = pDelim+1;
		}
	}
}

#if defined(OS_WIN32)
#ifndef snprintf
#define snprintf sprintf_s
#endif

#ifndef strnicmp
#define strnicmp _strnicmp
#endif

#endif  // OS_WIN32

template <class Type>
void WriteToText(const Type&, char*, long)
{
	//assert(false); // this type is unsupported
}

template <>
inline void WriteToText<char>(const char& val, char* text, long max)
{
	snprintf(text, max, "%s", &val);
}

template <>
inline void WriteToText<bool>(const bool& val, char* text, long max)
{
	snprintf(text, max, "%s", val ? "yes" : "no");
}

template <>
inline void WriteToText<float>(const float& val, char* text, long max)
{
	snprintf(text, max, "%.6g", val);
}

template <>
inline void WriteToText<double>(const double& val, char* text, long max)
{
	snprintf(text, max, "%.8g", val);
}

template <>
inline void WriteToText<int>(const int& val, char* text, long max)
{
	snprintf(text, max, "%d", val);
}

#if defined(GLM_VERSION)
// ************************************
// Helpers for reading glm::vec types from config.
// ************************************
template<>
inline void ReadFromText<glm::vec3>(glm::vec3* val, const char* const text)
{
	ReadFromText((float(*)[3]) val, text);
}
template<>
inline void ReadFromText<glm::vec4>(glm::vec4* val, const char* const text)
{
	ReadFromText((float(*)[4]) val, text);
}
template<>
inline void WriteToText<glm::vec3>(const glm::vec3& val, char* text, long max)
{
	snprintf(text, max, "{%f,%f,%f}", val.x, val.y, val.z);
}
template<>
inline void WriteToText<glm::vec4>(const glm::vec4& val, char* text, long max)
{
	snprintf(text, max, "{%f,%f,%f,%f}", val.x, val.y, val.z, val.w);
}
#endif // defined(GLM_VERSION)

template <class Type>
class Variable : public VariableBase {
private:
	Type*            mPtr;
public:

	explicit Variable(const char* name, Type* ptr, unsigned long flags = 0, void* data = 0)
		: VariableBase(name, flags)
	{
		mPtr = ptr;
	}

	virtual ~Variable() {}

	virtual void SetValue(const char* const text)
	{
		ReadFromText(mPtr, text);
		SetFlags(GetFlags() | kVariableModified);
	}

	virtual void GetValue(char* text, long max) const
	{
		WriteToText(*mPtr, text, max);
	}
};

extern VariableBase* GetVariable(const char* name);
extern Map<VariableBase>* GetAllVariables();
extern bool AddVariable(VariableBase* variable);

template <typename Type>
Type RegisterVariable(const char* name, Type* ptr, Type init, unsigned long flags)
{
	VariableBase* variable = GetVariable(name);
	if (!variable) {
		variable = new Variable<Type>(name, ptr, flags, 0);
		AddVariable(variable);
	}
	else {
		variable->SetFlags(flags);
	}

	return init;
}

/// Define and register a variable for use with the configuration file
#define VAR(type, var, val, flags) \
    type var = (type)(val); \
    static type var##_Var = RegisterVariable<type>(#var, (type*)&var, (type)(val), flags)

#define EXTERN_VAR(type, var) \
    extern "C" { extern type var; }

/// @}	// End doxygen group

#endif //_SYS_CONFIG_H_