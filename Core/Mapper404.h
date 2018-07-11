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

	protected:
        uint8_t _regShadow[3];
		uint8_t _irqReloadValue;
		uint8_t _irqCounter;
		bool _irqReload;
		bool _irqEnabled;
		virtual uint16_t GetPRGPageSize() override { return 0x8000; }
		virtual uint16_t GetCHRPageSize() override { return 0x2000; }
		virtual uint32_t GetSaveRamPageSize() override { return 0x2000; }
		virtual uint32_t GetSaveRamSize() override { return 0x2000; }

		virtual void StreamState(bool saving) override
		{
			BaseMapper::StreamState(saving);
			ArrayInfo<uint8_t> registers = { _regShadow, 3 };
			SnapshotInfo a12Watcher{ &_a12Watcher };
			Stream(_irqReloadValue, _irqCounter, _irqReload, _irqEnabled, a12Watcher,
				_wramEnabled, _wramWriteProtected, registers);
		}

		void AfterLoadState() override
		{
            SelectChrBank();
            SelectPrgBank();
            UpdateRamMap();
		}

		void InitMapper() override 
		{
            // Initialize internal state to random values
            _regShadow[0] = GetPowerOnByte();
            _regShadow[1] = GetPowerOnByte() & 0x3;
            _regShadow[2] = GetPowerOnByte();
            _irqReloadValue = GetPowerOnByte();
            _irqCounter = GetPowerOnByte();
            _irqEnabled = GetPowerOnByte() & 1;
            _wramEnabled = GetPowerOnByte() & 1;
            _wramWriteProtected = GetPowerOnByte() & 1;
            AfterLoadState();
		}

        void UpdateRamMap(void)
        {
            if (_wramEnabled) {
      			SetCpuMemoryMapping(0x6000, 0x7FFF, 0, HasBattery() ? PrgMemoryType::SaveRam : PrgMemoryType::WorkRam,
                        _wramWriteProtected ? MemoryAccessType::Read : MemoryAccessType::ReadWrite);
            } else {
                RemoveCpuMemoryMapping(0x6000, 0x7FFF);
            }
        }

        void SelectChrBank(void)
        {
            SelectCHRPage(0, _regShadow[2]);
        }

        void SelectPrgBank(void)
        {
   			SelectPRGPage(0, _regShadow[0] | (_regShadow[1]<<8));
        }

		void WriteRegister(uint16_t addr, uint8_t value) override
		{
            switch((NFRomRegisters)(addr & 0xE001)) {
				case NFRomRegisters::Reg8000:   // CHR bank, low byte
                    _regShadow[0] = value;
                    SelectPrgBank();
					break;

				case NFRomRegisters::Reg8001:   // CHR bank, high bits
                    _regShadow[1] = value & 0x3;
        			SelectPRGPage(0, _regShadow[0] | (_regShadow[1]<<8));
					break;

				case NFRomRegisters::RegA000:   // PRG bank
                    _regShadow[2] = value;
                    SelectChrBank();
					break;

				case NFRomRegisters::RegA001:   // Mirroring and RAM options
                    // - BIT0, mirroring: 0 vertical, 1 horizontall.
                    // - BIT6, RAM write enable: 0 allow writes, 1 prohibit writes.
                    // - BIT7, RAM enable: 0: RAM disabled, 1: RAM enabled.
                    _wramEnabled = value & 0x80 ? 1 : 0;
                    _wramWriteProtected = value & 0x40 ? 1 : 0;
                    SetMirroringType(value & 1 ? MirroringType::Horizontal : MirroringType::Vertical);
					break;

				case NFRomRegisters::RegC000:   // IRQ latch
					_irqReloadValue = value;
					break;

				case NFRomRegisters::RegC001:   // IRQ reload
					_irqCounter = 0;
					_irqReload = true;
					break;

				case NFRomRegisters::RegE000:   // IRQ disable
					_irqEnabled = false;
					_console->GetCpu()->ClearIrqSource(IRQSource::External);
					break;

				case NFRomRegisters::RegE001:   // IRQ enable
					_irqEnabled = true;
					break;
			}
		}
	public:
		virtual void NotifyVRAMAddressChange(uint16_t addr) override
		{
			switch(_a12Watcher.UpdateVramAddress(addr, _console->GetPpu()->GetFrameCycle())) {
				case A12StateChange::None:
				case A12StateChange::Fall:
					break;

				case A12StateChange::Rise:
					if(_irqCounter == 0 || _irqReload) {
						_irqCounter = _irqReloadValue;
					} else {
						_irqCounter--;
					}

					if(_irqCounter == 0 && _irqEnabled) {
        				_console->GetCpu()->SetIrqSource(IRQSource::External);
					}
					_irqReload = false;
					break;
			}
		}
};
