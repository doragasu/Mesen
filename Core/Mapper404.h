#pragma once
#include "stdafx.h"
#include "BaseMapper.h"
#include "CPU.h"
#include "A12Watcher.h"

class Mapper404 : public BaseMapper
{
	private: 
		enum class NFRomRegisters
		{
			Reg8000 = 0x8000,
			Reg8001 = 0x8001,
			RegA000 = 0xA000,
			RegA001 = 0xA001,
			RegC000 = 0xC000,
			RegC001 = 0xC001,
			RegE000 = 0xE000,
			RegE001 = 0xE001
		};

		bool _wramEnabled;
		bool _wramWriteProtected;

		A12Watcher _a12Watcher;
		bool _needIrq;

	protected:
		uint8_t _irqReloadValue;
		uint8_t _irqCounter;
		bool _irqReload;
		bool _irqEnabled;
		virtual uint16_t GetPRGPageSize() override { return 0x8000; }
		virtual uint16_t GetCHRPageSize() override { return 0x2000; }

        /// TODO
		void InitMapper() override 
		{
			SelectPRGPage(0, GetPowerOnByte() & 0x03);
			SelectCHRPage(0, GetPowerOnByte() & 0x03);
		}

		void WriteRegister(uint16_t addr, uint8_t value) override
		{
            switch((NFRomRegisters)(addr & 0xE001)) {
				case NFRomRegisters::Reg8000:
					_state.Reg8000 = value;
					UpdateState();
					break;

				case NFRomRegisters::Reg8001:
					if(_currentRegister <= 1) {
						//"Writes to registers 0 and 1 always ignore bit 0"
						value &= ~0x01;
					}
					_registers[_currentRegister] = value;
					UpdateState();
					break;

				case NFRomRegisters::RegA000:
					_state.RegA000 = value;
					UpdateMirroring();
					break;

				case NFRomRegisters::RegA001:
					_state.RegA001 = value;
					UpdateState();
					break;

				case NFRomRegisters::RegC000:
					_irqReloadValue = value;
					break;

				case NFRomRegisters::RegC001:
					_irqCounter = 0;
					_irqReload = true;
					break;

				case NFRomRegisters::RegE000:
					_irqEnabled = false;
					_console->GetCpu()->ClearIrqSource(IRQSource::External);
					break;

				case NFRomRegisters::RegE001:
					_irqEnabled = true;
					break;
			}

            /// TODO
			SelectPRGPage(0, (value >> 4) & 0x03);
			SelectCHRPage(0, value & 0x03);
		}
	public:
		virtual void NotifyVRAMAddressChange(uint16_t addr) override
		{
			switch(_a12Watcher.UpdateVramAddress(addr, _console->GetPpu()->GetFrameCycle())) {
				case A12StateChange::None:
					break;

				case A12StateChange::Fall:
					if(_needIrq) {
						//Used by MC-ACC (Acclaim copy of the MMC3), see TriggerIrq above
						_console->GetCpu()->SetIrqSource(IRQSource::External);
						_needIrq = false;
					}
					break;
				case A12StateChange::Rise:
					uint32_t count = _irqCounter;
					if(_irqCounter == 0 || _irqReload) {
						_irqCounter = _irqReloadValue;
					} else {
						_irqCounter--;
					}

					//SubMapper 2 = MC-ACC (Acclaim MMC3 clone)
					if(!IsMcAcc() && (ForceMmc3RevAIrqs() || EmulationSettings::CheckFlag(EmulationFlags::Mmc3IrqAltBehavior))) {
						//MMC3 Revision A behavior
						if((count > 0 || _irqReload) && _irqCounter == 0 && _irqEnabled) {
							TriggerIrq();
						}
					} else {
						if(_irqCounter == 0 && _irqEnabled) {
							TriggerIrq();
						}
					}
					_irqReload = false;
					break;
			}
		}
};
