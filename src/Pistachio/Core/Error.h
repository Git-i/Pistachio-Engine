#pragma once
#include "FormatsAndTypes.h"
#include "ptpch.h"
#include "Log.h"
#include <filesystem>
#include <utility>
#include "pktx/texture.h"
#ifdef __GNUC__
#define PT_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif
namespace Pistachio {
	enum class ErrorType
	{
		Success,
		Unknown, 
		InvalidFile,
		NonExistentFile,
		OutOfMemory,
		ProvidedInString,
		ParameterError,
		InvalidResourceType
	};
	class Error {
	public:
		Error(ErrorType t) :type(t) {}
		Error(): type(ErrorType::Success){}
		static Error FromRHIError(RHI::CreationError e)
		{
			ErrorType t;
			switch (e) 
			{
				case (RHI::CreationError::OutOfDeviceMemory): [[fallthrough]];
				case (RHI::CreationError::OutOfHostMemory): t = ErrorType::OutOfMemory;
					break;
				case (RHI::CreationError::InvalidParameters): t = ErrorType::ParameterError;
					break;
				default: t = ErrorType::Unknown;
			}
			return {t};
		}
		static Error FromKTXError(ktx_error_code_e e)
		{
			return {ErrorType::Unknown}; //todo
		}
		Error(ErrorType type, std::string detail) :type(type),detailString(std::move(detail)){ };
		static std::string GetErrorString(const Error& e) {
			switch (e.GetErrorType())
			{
			case ErrorType::Unknown:
				return "Unkown Internal Error";
			case ErrorType::NonExistentFile:
				return (std::string("The file passed into the function doesn't exist: ") + std::string(e.GetDetailString()));
			case ErrorType::InvalidResourceType:
				return (std::string("Invalid Resource Type ") + std::string(e.GetDetailString()));
			case ErrorType::ProvidedInString:
				return e.GetDetailString();
			default: return "Unregistered Error Type";
				break;
			}
		};
		static void LogErrorToConsole(const Error & e) {
			PT_CORE_ERROR(GetErrorString(e));
		};
		static void Assert(const Error & e) {
			LogErrorToConsole(e);
			PT_CORE_ASSERT(e.Successful());
		};
		[[nodiscard]] ErrorType GetErrorType() const { return type; };
		[[nodiscard]] bool Successful() const {return type == ErrorType::Success;}
		[[nodiscard]] const std::string& GetDetailString() const { return detailString; };
	private:
		ErrorType type;
		std::string detailString;
	};
	template <typename T>
	using Result = ezr::result<T, Error>;
}

