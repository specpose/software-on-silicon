#pragma once
#include<stdexcept>

namespace SFA {
    namespace util{
		enum error_code {
			noerror,
			InvalidDMAObjectSize,
			DMADescriptorIdIsReservedForTheSerialLineIdleState,
			DMADescriptorIdIsReservedForTheComShutdownRequestOnIdle,
			DMADescriptorIdIsReservedForThePoweronNotification,
			DMADescriptorIdIsReservedForTheReadsFinishedNotification,
			PoweronAfterUnexpectedShutdown,
			HotplugAfterUnexpectedShutdown,
			ObjectCouldBeOutdated,
			PowerOnWithPendingComShutdown,
			PreviousTransferRequestsWereNotCleared,
			DuplicateComShutdown,
			DuplicateSighup,
			DuplicateReadlockRequest,
			SyncedObjectsAreNotSupposedToHaveaTransfer,
			AcknowledgeReceivedWithoutAnyRequest,
			ReceivedATransferAcknowledgeOnSyncedObject,
			ReceivedATransferAcknowledgeOnReadlockedObject,
			ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer,
			ReadlockPredatesAcknowledge,
			AcknowledgeIdDoesNotReferenceAValidObject, //2x
			PreviousTransferHasNotBeenAcknowledged,
			InvalidAcknowledgeId,
			SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired,
			DMAObjectHasEnteredAnIllegalSyncState,
			FoundATransferObjectWhichIsSynced,
			FoundATransferObjectWhichIsReadlocked,
			PreviousObjectWriteHasNotBeenCompleted,
			PreviousReadobjectHasNotFinished,
			NoIdleReceivedAndNoReceivelockObtained,
			DMADescriptorsInitializationFailed, //2x
			FPGAProcessingSwitchThreadIsTooSlow,
			MCUProcessingSwitchRelaunchedAfterComHotplugAction,
			MCUProcessingSwitchThreadIsTooSlow,
			BitsetDoesNotFitIntoChararray,
			NumericValueOfBitsetDoesNotFitIntoChararray,
			CombufferSizeIsMinimumWORDSIZE,
			CombufferInAndOutSizeNotEqual,
			AttemptedReadAfterEndOfBuffer,
			AttemptedWriteAfterEndOfBuffer,
			WritebyteCanOnlyBeCalledOnceInSerialeventloop //2x
		};
		const char * error_message(error_code what){
			switch (what){
				case error_code::noerror: return "No error was supplied after initialization of static error variable";
				case error_code::InvalidDMAObjectSize : return "Invalid DMAObject size";
				case error_code::DMADescriptorIdIsReservedForTheSerialLineIdleState :return "DMADescriptor id is reserved for the serial line idle state";
				case error_code::DMADescriptorIdIsReservedForTheComShutdownRequestOnIdle : return "DMADescriptor id is reserved for the com_shutdown request on idle";
				case error_code::DMADescriptorIdIsReservedForThePoweronNotification : return "DMADescriptor id is reserved for the poweron notification";
				case error_code::DMADescriptorIdIsReservedForTheReadsFinishedNotification : return "DMADescriptor id is reserved for the readsFinished notification";
				case error_code::PoweronAfterUnexpectedShutdown : return "Poweron after unexpected shutdown";
				case error_code::HotplugAfterUnexpectedShutdown : return "Hotplug after unexpected shutdown";
				case error_code::ObjectCouldBeOutdated : return "Object could be outdated. Corrupted unless resend_current_object is called from the other side";
				case error_code::PowerOnWithPendingComShutdown : return "Power on with pending com_shutdown";
				case error_code::PreviousTransferRequestsWereNotCleared : return "Previous transfer requests were not cleared";
				case error_code::DuplicateComShutdown : return "Duplicate comShutdown";
				case error_code::DuplicateSighup : return "Duplicate sighup";
				case error_code::DuplicateReadlockRequest : return "Duplicate readLock request";
				case error_code::SyncedObjectsAreNotSupposedToHaveaTransfer : return "Synced objects are not supposed to have a transfer";
				case error_code::AcknowledgeReceivedWithoutAnyRequest : return "Acknowledge received without any request";
				case error_code::ReceivedATransferAcknowledgeOnSyncedObject: return "Received a transfer acknowledge on synced object";
				case error_code::ReceivedATransferAcknowledgeOnReadlockedObject : return "Received a transfer acknowledge on readLocked object";
				case error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer : return "Received a duplicate transfer acknowledge on object in transfer";
				case error_code::ReadlockPredatesAcknowledge : return "ReadLock pre-dates acknowledge";
				case error_code::AcknowledgeIdDoesNotReferenceAValidObject : return "AcknowledgeId does not reference a valid object";
				case error_code::PreviousTransferHasNotBeenAcknowledged : return "Previous transfer has not been acknowledged";
				case error_code::InvalidAcknowledgeId : return "Invalid acknowledgeId";
				case error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired : return "Synced status has not been overriden when readLock was acquired";
				case error_code::DMAObjectHasEnteredAnIllegalSyncState : return "DMAObject has entered an illegal sync state";
				case error_code::FoundATransferObjectWhichIsSynced : return "Found a transfer object which is synced";
				case error_code::FoundATransferObjectWhichIsReadlocked : return "Found a transfer object which is readLocked";
				case error_code::PreviousObjectWriteHasNotBeenCompleted : return "Previous object write has not been completed";
				case error_code::PreviousReadobjectHasNotFinished : return "Previous read object has not finished";
				case error_code::NoIdleReceivedAndNoReceivelockObtained : return "No idle received and no receive_lock obtained";
				case error_code::DMADescriptorsInitializationFailed : return "DMADescriptors initialization failed";
				case error_code::FPGAProcessingSwitchThreadIsTooSlow : return "FPGAProcessingSwitch thread is too slow";
				case error_code::MCUProcessingSwitchRelaunchedAfterComHotplugAction : return "Testing MCU hotplug: MCU ProcessingSwitch relaunched after com_hotplug_action";
				case error_code::MCUProcessingSwitchThreadIsTooSlow : return "MCUProcessingSwitch thread is too slow";
				case error_code::BitsetDoesNotFitIntoChararray : return "BitSet does not fit into char array";
				case error_code::NumericValueOfBitsetDoesNotFitIntoChararray : return "Numeric value of BitSet does not fit into char array";
				case error_code::CombufferSizeIsMinimumWORDSIZE : return "ComBuffer size is minimum WORD_SIZE";
				case error_code::CombufferInAndOutSizeNotEqual : return "ComBuffer in and out size not equal";
				case error_code::AttemptedReadAfterEndOfBuffer : return "Attempted read after end of buffer";
				case error_code::AttemptedWriteAfterEndOfBuffer : return "Attempted write after end of buffer";
				case error_code::WritebyteCanOnlyBeCalledOnceInSerialeventloop : return "write_byte can only be called once in Serial event_loop";
			}
			return "No error was supplied after initialization of static SFA::util::error";
		}
		static error_code error = error_code::noerror;
		void logic_error(const char* modname, error_code what, std::string file_name, std::string function_name) {
			if (error == error_code::noerror)
				error = what;
			std::cout << modname << " Logic Error: " << error_message(error) << ". " << file_name << "::" << function_name << std::endl;
			abort();
		};
		void runtime_error(const char* modname, error_code what, std::string file_name, std::string function_name) {
			if (error == error_code::noerror)
				error = what;
			std::cout << modname << " Runtime Error: " << error_message(error) << ". " << file_name << "::" << function_name  << std::endl;
			abort();
		};
    }
}
