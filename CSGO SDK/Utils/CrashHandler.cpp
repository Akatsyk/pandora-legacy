#include "CrashHandler.hpp"
#include "LogSystem.hpp"

class CrashHandler : public ICrashHandler {
public:
   long __stdcall OnCrashProgramm( struct _EXCEPTION_POINTERS* ) override;

private:
};

Encrypted_t<ICrashHandler> ICrashHandler::Get( ) {
   static CrashHandler instance;
   return &instance;
}

long __stdcall CrashHandler::OnCrashProgramm( struct _EXCEPTION_POINTERS* ExceptionInfo ) {
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "---------------------------------" ) );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "base: 0x%p" ), g_Vars.globals.hModule );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "exception at: 0x%p, magic: %s" ), ExceptionInfo->ExceptionRecord->ExceptionAddress, g_Vars.globals.szLastHookCalled.c_str( ) );

   int m_ExceptionCode = ExceptionInfo->ExceptionRecord->ExceptionCode;
   int m_exceptionInfo_0 = ExceptionInfo->ExceptionRecord->ExceptionInformation[ 0 ];
   int m_exceptionInfo_1 = ExceptionInfo->ExceptionRecord->ExceptionInformation[ 1 ];
   int m_exceptionInfo_2 = ExceptionInfo->ExceptionRecord->ExceptionInformation[ 2 ];

   switch ( m_ExceptionCode ) {
	  case EXCEPTION_ACCESS_VIOLATION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_ACCESS_VIOLATION" ) );
	  if ( m_exceptionInfo_0 == 0 ) {
		 // bad read
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Attempted to read from: 0x%08x" ), m_exceptionInfo_1 );
	  } else if ( m_exceptionInfo_0 == 1 ) {
		 // bad write
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Attempted to write to: 0x%08x" ), m_exceptionInfo_1 );
	  } else if ( m_exceptionInfo_0 == 8 ) {
		 // user-mode data execution prevention (DEP)
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Data Execution Prevention (DEP) at: 0x%08x" ), m_exceptionInfo_1 );
	  } else {
		 // unknown, shouldn't happen
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Unknown access violation at: 0x%08x" ), m_exceptionInfo_1 );
	  }
	  break;

	  case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_ARRAY_BOUNDS_EXCEEDED" ) );
	  break;

	  case EXCEPTION_BREAKPOINT:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_BREAKPOINT" ) );
	  break;

	  case EXCEPTION_DATATYPE_MISALIGNMENT:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_DATATYPE_MISALIGNMENT" ) );
	  break;

	  case EXCEPTION_FLT_DENORMAL_OPERAND:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_DENORMAL_OPERAND" ) );
	  break;

	  case EXCEPTION_FLT_DIVIDE_BY_ZERO:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_DIVIDE_BY_ZERO" ) );
	  break;

	  case EXCEPTION_FLT_INEXACT_RESULT:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_INEXACT_RESULT" ) );
	  break;

	  case EXCEPTION_FLT_INVALID_OPERATION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_INVALID_OPERATION" ) );
	  break;

	  case EXCEPTION_FLT_OVERFLOW:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_OVERFLOW" ) );
	  break;

	  case EXCEPTION_FLT_STACK_CHECK:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_STACK_CHECK" ) );
	  break;

	  case EXCEPTION_FLT_UNDERFLOW:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_FLT_UNDERFLOW" ) );
	  break;

	  case EXCEPTION_ILLEGAL_INSTRUCTION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_ILLEGAL_INSTRUCTION" ) );
	  break;

	  case EXCEPTION_IN_PAGE_ERROR:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_IN_PAGE_ERROR" ) );
	  if ( m_exceptionInfo_0 == 0 ) {
		 // bad read
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Attempted to read from: 0x%08x" ), m_exceptionInfo_1 );
	  } else if ( m_exceptionInfo_0 == 1 ) {
		 // bad write
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Attempted to write to: 0x%08x" ), m_exceptionInfo_1 );
	  } else if ( m_exceptionInfo_0 == 8 ) {
		 // user-mode data execution prevention (DEP)
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Data Execution Prevention (DEP) at: 0x%08x" ), m_exceptionInfo_1 );
	  } else {
		 // unknown, shouldn't happen
		 ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "Unknown access violation at: 0x%08x" ), m_exceptionInfo_1 );
	  }

	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "NTSTATUS: 0x%08x" ), m_exceptionInfo_2 );
	  break;

	  case EXCEPTION_INT_DIVIDE_BY_ZERO:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_INT_DIVIDE_BY_ZERO" ) );
	  break;

	  case EXCEPTION_INT_OVERFLOW:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_INT_OVERFLOW" ) );
	  break;

	  case EXCEPTION_INVALID_DISPOSITION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_INVALID_DISPOSITION" ) );
	  break;

	  case EXCEPTION_NONCONTINUABLE_EXCEPTION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_NONCONTINUABLE_EXCEPTION" ) );
	  break;

	  case EXCEPTION_PRIV_INSTRUCTION:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_PRIV_INSTRUCTION" ) );
	  break;

	  case EXCEPTION_SINGLE_STEP:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_SINGLE_STEP" ) );
	  break;

	  case EXCEPTION_STACK_OVERFLOW:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: EXCEPTION_STACK_OVERFLOW" ) );
	  break;

	  case DBG_CONTROL_C:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: DBG_CONTROL_C" ) );
	  break;

	  default:
	  ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "CODE: %08x" ), m_ExceptionCode );
   }

   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "---------------------------------" ) );

   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "asm registers:" ) );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "eax: 0x%08x | esi: 0x%08x" ), ExceptionInfo->ContextRecord->Eax, ExceptionInfo->ContextRecord->Esi );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "ebx: 0x%08x | edi: 0x%08x" ), ExceptionInfo->ContextRecord->Ebx, ExceptionInfo->ContextRecord->Edi );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "ecx: 0x%08x | ebp: 0x%08x" ), ExceptionInfo->ContextRecord->Ecx, ExceptionInfo->ContextRecord->Ebp );
   ILogSystem::Get( )->Log( XorStr( ".ams" ), XorStr( "edx: 0x%08x | esp: 0x%08x" ), ExceptionInfo->ContextRecord->Edx, ExceptionInfo->ContextRecord->Esp );

   return EXCEPTION_CONTINUE_SEARCH;
}
