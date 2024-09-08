#pragma once
#include "FormatsAndTypes.h"
#include "ptpch.h"
#include "Log.h"
#include <filesystem>
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
		Error(ErrorType t) {
			type = t;
			severity = GetErrorSeverity(type);
			ReporterString = "Unknown Internal Error";
		}
		inline static bool CheckFileExistence(const char* filepath) {
			if (std::filesystem::exists(filepath))
				return true;
			return false;
		}
		Error()
		{
			*this = Error(ErrorType::Success, "");
		}
		static Error FromRHIError(RHI::CreationError e, const std::string& str = "")
		{
			bool report = !str.empty();
			ErrorType t = ErrorType::Unknown;
			switch (e) 
			{
				case (RHI::CreationError::OutOfDeviceMemory): [[fallthrough]];
				case (RHI::CreationError::OutOfHostMemory): t = ErrorType::OutOfMemory;
					break;
				case (RHI::CreationError::InvalidParameters): t = ErrorType::ParameterError;
					break;
				default: t = ErrorType::Unknown;
			}
			return report ? Error(t, str) : Error(t);
		}
		static Error FromKTXError(ktx_error_code_e e, const std::string& str = "")
		{
			return Error(ErrorType::Unknown); //todo
		}
		Error(ErrorType etype, const std::string& funcsig) :type(etype),ReporterString(funcsig), severity(GetErrorSeverity(etype)){ };
		inline static std::string GetErrorString(const Error& e) {
			switch (e.GetErrorType())
			{
			case Pistachio::ErrorType::Unknown:
				return "Unkown Internal Error";
			case Pistachio::ErrorType::NonExistentFile:
				return (std::string("The file passed into the function doesn't exist: ") + std::string(e.GetReporterString()));
			case Pistachio::ErrorType::InvalidResourceType:
				return (std::string("Resource Type used does not exist: ") + std::string(e.GetReporterString()));
			case Pistachio::ErrorType::ProvidedInString:
				return e.GetReporterString();
			default: return "Unregistered Error Type";
				break;
			}
		};
		inline static void LogErrorToConsole(const Error & e) {
			if (e.GetSeverity() == 1)
				PT_CORE_WARN(GetErrorString(e));
			if (e.GetSeverity() == 2)
				PT_CORE_ERROR(GetErrorString(e));
		};
		inline static void Assert(const Error & e) {
			LogErrorToConsole(e);
			PT_CORE_ASSERT(e.GetSeverity() != 2);
		};
		ErrorType GetErrorType() const { return type; };
		bool Successful() const {return type == ErrorType::Success;}
		const char* GetReporterString() const { return ReporterString.c_str(); };
		static int GetErrorSeverity(ErrorType type){
			switch (type)
			{
			case Pistachio::ErrorType::Unknown: return 2;
				break;
			case Pistachio::ErrorType::Success: return 0;
				break;
			case Pistachio::ErrorType::ParameterError: return 2;
				break;
			case Pistachio::ErrorType::NonExistentFile: return 2;
				break;
			case Pistachio::ErrorType::OutOfMemory: return 2;
			default: return 0;
				break;
			}
		}
		int GetSeverity() const { return severity; };
	private:
		ErrorType type;
		std::string ReporterString;
		int severity;
	};
	template <typename T>
	using Result = ezr::result<T, Error>;
}
class Reporter {
	Reporter(std::string& caller);
};
