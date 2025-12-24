#pragma once
#include<stdexcept>

namespace SFA {
    namespace util{
		enum error_code {
			noerror,
//SerialDMA
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
			SyncedObjectsAreNotSupposedToHaveaTransfer,
			IncomingReadlockIsCancelingLocalWriteOperation,
			IncomingReadlockIsRejectedOrOmitted,
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
			WritebyteCanOnlyBeCalledOnceInSerialeventloop, //2x
//RingBuffer.hpp
			RequestedRingbufferSizeNotBigEnough,
//ringbuffer_hellpers.hpp
			IndividualWritelengthTooBigOrRingBufferTooSmall,
			RingbufferTooSlowOrNotBigEnough,
//MemoryController.cpp
			NegativeReadoffsetSupplied, //2x
			ReadindexOutOfBounds, //2x
//MemoryController.hpp
			InvalidReadDestination,
			WriterBufferFull
//RingToMemory.cpp
		};
		const std::string error_message(error_code what){
			switch (what){
				case error_code::noerror: return std::string("No error was supplied after initialization of static error variable");
				case error_code::InvalidDMAObjectSize : return std::string("Invalid DMAObject size");
				case error_code::DMADescriptorIdIsReservedForTheSerialLineIdleState :return std::string("DMADescriptor id is reserved for the serial line idle state");
				case error_code::DMADescriptorIdIsReservedForTheComShutdownRequestOnIdle : return std::string("DMADescriptor id is reserved for the com_shutdown request on idle");
				case error_code::DMADescriptorIdIsReservedForThePoweronNotification : return std::string("DMADescriptor id is reserved for the poweron notification");
				case error_code::DMADescriptorIdIsReservedForTheReadsFinishedNotification : return std::string("DMADescriptor id is reserved for the readsFinished notification");
				case error_code::PoweronAfterUnexpectedShutdown : return std::string("Poweron after unexpected shutdown");
				case error_code::HotplugAfterUnexpectedShutdown : return std::string("Hotplug after unexpected shutdown");
				case error_code::ObjectCouldBeOutdated : return std::string("Object could be outdated. Corrupted unless resend_current_object is called from the other side");
				case error_code::PowerOnWithPendingComShutdown : return std::string("Power on with pending com_shutdown");
				case error_code::PreviousTransferRequestsWereNotCleared : return std::string("Previous transfer requests were not cleared");
				case error_code::DuplicateComShutdown : return std::string("Duplicate comShutdown");
				case error_code::DuplicateSighup : return std::string("Duplicate sighup");
				case error_code::SyncedObjectsAreNotSupposedToHaveaTransfer : return std::string("Synced objects are not supposed to have a transfer");
				case error_code::IncomingReadlockIsCancelingLocalWriteOperation : std::string("Incoming readLock is canceling local write operation");
				case error_code::IncomingReadlockIsRejectedOrOmitted : std::string("Incoming readLock is rejected or omitted");
				case error_code::AcknowledgeReceivedWithoutAnyRequest : return std::string("Acknowledge received without any request");
				case error_code::ReceivedATransferAcknowledgeOnSyncedObject: return std::string("Received a transfer acknowledge on synced object");
				case error_code::ReceivedATransferAcknowledgeOnReadlockedObject : return std::string("Received a transfer acknowledge on readLocked object");
				case error_code::ReceivedADuplicateTransferAcknowledgeOnObjectInTransfer : return std::string("Received a duplicate transfer acknowledge on object in transfer");
				case error_code::ReadlockPredatesAcknowledge : return std::string("ReadLock pre-dates acknowledge");
				case error_code::AcknowledgeIdDoesNotReferenceAValidObject : return std::string("AcknowledgeId does not reference a valid object");
				case error_code::PreviousTransferHasNotBeenAcknowledged : return std::string("Previous transfer has not been acknowledged");
				case error_code::InvalidAcknowledgeId : return std::string("Invalid acknowledgeId");
				case error_code::SyncedStatusHasNotBeenOverridenWhenReadlockWasAcquired : return std::string("Synced status has not been overriden when readLock was acquired");
				case error_code::DMAObjectHasEnteredAnIllegalSyncState : return std::string("DMAObject has entered an illegal sync state");
				case error_code::FoundATransferObjectWhichIsSynced : return std::string("Found a transfer object which is synced");
				case error_code::FoundATransferObjectWhichIsReadlocked : return std::string("Found a transfer object which is readLocked");
				case error_code::PreviousObjectWriteHasNotBeenCompleted : return std::string("Previous object write has not been completed");
				case error_code::PreviousReadobjectHasNotFinished : return std::string("Previous read object has not finished");
				case error_code::NoIdleReceivedAndNoReceivelockObtained : return std::string("No idle received and no receive_lock obtained");
				case error_code::DMADescriptorsInitializationFailed : return std::string("DMADescriptors initialization failed");
				case error_code::FPGAProcessingSwitchThreadIsTooSlow : return std::string("FPGAProcessingSwitch thread is too slow");
				case error_code::MCUProcessingSwitchRelaunchedAfterComHotplugAction : return std::string("Testing MCU hotplug: MCU ProcessingSwitch relaunched after com_hotplug_action");
				case error_code::MCUProcessingSwitchThreadIsTooSlow : return std::string("MCUProcessingSwitch thread is too slow");
				case error_code::BitsetDoesNotFitIntoChararray : return std::string("BitSet does not fit into char array");
				case error_code::NumericValueOfBitsetDoesNotFitIntoChararray : return std::string("Numeric value of BitSet does not fit into char array");
				case error_code::CombufferSizeIsMinimumWORDSIZE : return std::string("ComBuffer size is minimum WORD_SIZE");
				case error_code::CombufferInAndOutSizeNotEqual : return std::string("ComBuffer in and out size not equal");
				case error_code::AttemptedReadAfterEndOfBuffer : return std::string("Attempted read after end of buffer");
				case error_code::AttemptedWriteAfterEndOfBuffer : return std::string("Attempted write after end of buffer");
				case error_code::WritebyteCanOnlyBeCalledOnceInSerialeventloop : return std::string("write_byte can only be called once in Serial event_loop");
				case error_code::RequestedRingbufferSizeNotBigEnough : return std::string("Requested RingBuffer size not big enough.");
				case error_code::IndividualWritelengthTooBigOrRingBufferTooSmall : return std::string("Individual write length too big or RingBuffer too small");
				case error_code::RingbufferTooSlowOrNotBigEnough : return std::string("RingBuffer too slow or not big enough");
				case error_code::NegativeReadoffsetSupplied : return std::string("Negative read offset supplied");
				case error_code::ReadindexOutOfBounds : return std::string("Read index out of bounds");
				case error_code::InvalidReadDestination : return std::string("Invalid Read Destination");
				case error_code::WriterBufferFull : return std::string("Writer Buffer full");
			}
			return std::string("No error was supplied after initialization of static SFA::util::error");
		}
		static error_code error = error_code::noerror;
		void logic_error(error_code what, std::string file_name, std::string function_name, const char* modname = nullptr) {
			if (error == error_code::noerror)
				error = what;
			if (!modname)
				modname = "";
			std::cerr << std::endl << "Logic" << modname << "Error: " << error_message(error) << ". " << file_name << "::" << function_name << std::endl;
			abort();
		};
		void runtime_error(error_code what, std::string file_name, std::string function_name, const char* modname = nullptr) {
			if (error == error_code::noerror)
				error = what;
			if (!modname)
				modname = "";
			std::cerr << std::endl << "Runtime" << modname << "Error: " << error_message(error) << ". " << file_name << "::" << function_name  << std::endl;
			abort();
		};
    }
}
