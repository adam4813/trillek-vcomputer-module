#pragma once
/**
 * Trillek Virtual Computer - VComputer.hpp
 * Virtual Computer core
 */

#ifndef __VCOMPUTER_HPP_
#define __VCOMPUTER_HPP_ 1

#include "Types.hpp"

#include "ICPU.hpp"
#include "IDevice.hpp"
#include "AddrListener.hpp"

#include <vector>
#include <algorithm>
#include <memory>

#include <cassert>

namespace vm {
	using namespace vm::cpu;

	const unsigned MAX_N_DEVICES		= 32;					/// Max number of devices attached
	const std::size_t MAX_ROM_SIZE	= 32*1024;		/// Max ROM size
	const std::size_t MAX_RAM_SIZE	= 1024*1024;	/// Max RAM size

	class VComputer {
		public:

			/**
			 * Creates a Virtual Computer
			 * @param ram_size RAM size in BYTES
			 */
			VComputer (std::size_t ram_size = 128*1024) : 
				ram(nullptr), rom(nullptr), ram_size(ram_size), rom_size(0) {

					ram = new byte_t[ram_size];
				}

			~VComputer () {
				if (ram != nullptr)
					delete[] ram;
			}

			/**
			 * Sets the CPU of the computer
			 * @param cpu New CPU in the computer
			 */
			void SetCPU (std::unique_ptr<ICPU> cpu) {
				this->cpu = std::move(cpu);
				cpu->SetVComputer(this);
			}

			/**
			 * Removes the CPU of the computer
			 * @return Returns the ICPU
			 */
			std::unique_ptr<ICPU> RmCPU () {
				cpu->SetVComputer(nullptr);
				return std::move(cpu);
			}

			/**
			 * Adds a Device to a slot
			 * @param slot Were plug the device
			 * @param dev The device to be pluged in the slot
			 * @return False if the slot have a device or the slot is invalid.
			 */
			bool AddDevice (unsigned slot , std::shared_ptr<IDevice> dev) {
				if (slot >= MAX_N_DEVICES || !devices[slot]) {
					return false;
				}

				devices[slot] = dev;
				if (dev->IsSyncDev()) {
					sdevices[slot] = dev;
				} else {
					sdevices[slot].reset();
				}

				return true;
			}

			/**
			 * Gets the DEvice plugged in the slot
			 */
			std::shared_ptr<IDevice> GetDevice (unsigned slot) {
				if (slot >= MAX_N_DEVICES || !devices[slot]) {
					return nullptr;
				}

				return devices[slot];
			}


			/**
			 * Remove a device from a slot
			 * @param slot Slot were unplug the device
			 */
			void RmDevice (unsigned slot) {
				if (slot < MAX_N_DEVICES && devices[slot]) {
					devices[slot].reset(); // Cleans the slot
					sdevices[slot].reset();
				}
			}

			/**
			 * CPU clock speed in Hz
			 */
			unsigned CPUClock() const {
				if (cpu) {
					return cpu->Clock();
				}
				return 0;
			}

			/**
			 * Writes a copy of CPU state in a chunk of memory pointer by ptr.
			 * @param ptr Pointer were to write
			 * @param size Size of the chunk of memory were can write. If is 
			 * sucesfull, it will be set to the size of the write data.
			 */
			inline void GetState (void* ptr, std::size_t size) const {
				if (cpu) {
					return cpu->GetState(ptr, size);
				}
			}

			/**
			 * Gets a pointer were is stored the ROM data
			 * @param *rom Ptr to the ROM data
			 * @param rom_size Size of the ROM data that must be less or equal to 32KiB. Big sizes will be ignored
			 */
			void SetROM (const byte_t* rom, std::size_t rom_size) {
				assert (rom != nullptr);
				assert (rom_size > 0);

				this->rom = rom;
				this->rom_size = (rom_size > MAX_ROM_SIZE) ? MAX_ROM_SIZE : rom_size; 
			}

			/**
			 * Resets the virtual machine (but not clears RAM!)
			 */
			void Reset() {
				if (cpu) {
					cpu->Reset();
				}
			}

			/**
			 * Executes one instruction
			 * @param delta Number of milliseconds since the last call
			 * @return number of cycles executed
			 */
			unsigned Step( const double delta = 0) {
				if (cpu) {
					unsigned cycles = cpu->Step();
					//timers.Update(cycles);

					word_t msg;
					bool interrupted = false;//= timers.DoesInterrupt(msg);
					for (std::size_t i=0; i < MAX_N_DEVICES; i++) {
						if (!devices[i]) {
							continue; // Slot without device
						}

						// Does the sync job
						if (sdevices[i]) {
							sdevices[i]->Tick(cycles, delta);
						}

						// Try to get the highest priority interrupt
						if (! interrupted && devices[i]->DoesInterrupt(msg) ) { 
							interrupted = true;
							if (cpu->SendInterrupt(msg)) { // Send the interrupt to the CPU
								devices[i]->IACK(); // Informs to the device that his interrupt has been accepted by the CPU
							}
						}
					}


					return cycles;
				}

				return 0;
			}

			/**
			 * Executes N clock ticks
			 * @param n nubmer of clock ticks, by default 1
			 * @param delta Number of milliseconds since the last call
			 */
			void Tick( unsigned n=1, const double delta = 0) {
				assert(n >0);
				if (cpu) {
					cpu->Tick(n);
					//timers.Update(n);

					word_t msg;
					bool interrupted =false; // timers.DoesInterrupt(msg);
					for (std::size_t i=0; i < MAX_N_DEVICES; i++) {
						if (!devices[i]) {
							continue; // Slot without device
						}

						// Does the sync job
						if (sdevices[i]) {
							sdevices[i]->Tick(n, delta);
						}

						// Try to get the highest priority interrupt
						if (! interrupted && devices[i]->DoesInterrupt(msg) ) { 
							interrupted = true;
							if (cpu->SendInterrupt(msg)) { // Send the interrupt to the CPU
								devices[i]->IACK(); // Informs to the device that his interrupt has been accepted by the CPU
							}
						}
					}

				}		
			}

			byte_t ReadB (dword_t addr) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (!(addr & 0xFF0000 )) { // RAM address
					return ram[addr];
				}
				
				if ((addr & 0xFF0000) == 0x100000 ) { // ROM (0x100000-0x10FFFF)
					return rom[addr & 0x00FFFF];
				}

				// TODO
				return 0;
			}

			word_t ReadW (dword_t addr) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (!(addr & 0xFF0000 )) { // RAM address
					return ((word_t*)ram)[addr];
				}
				
				if ((addr & 0xFF0000) == 0x100000 ) { // ROM (0x100000-0x10FFFF)
					return ((word_t*)rom)[addr & 0x00FFFF];
				}
				// TODO
				return 0;
			}

			dword_t ReadDW (dword_t addr) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (!(addr & 0xFF0000 )) { // RAM address
					return ((dword_t*)ram)[addr];
				}
				
				if ((addr & 0xFF0000) == 0x100000 ) { // ROM (0x100000-0x10FFFF)
					return ((dword_t*)rom)[addr & 0x00FFFF];
				}
				// TODO
				return 0;
			}

			void WriteB (dword_t addr, byte_t val) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (addr < ram_size) { // RAM address
					ram[addr] = val;
				}
				
				// TODO
				
				return ; // You can't write in the ROM or not mapped addresses !

			}

			void WriteW (dword_t addr, word_t val) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (addr < ram_size-1 ) { // RAM address
					((word_t*)ram)[addr] = val;
				}
				// TODO What hapens when there is a write that falls half in RAM and
				// half outside ?
				// I actually forbid these cases to avoid buffer overun, but should be
				// allowed
				
				// TODO
			}

			void WriteDW (dword_t addr, dword_t val) {
				addr = addr & 0x00FFFFFF; // We use only 24 bit addresses

				if (addr < ram_size-3 ) { // RAM address
					((dword_t*)ram)[addr] = val;
				}
				// TODO What hapens when there is a write that falls half in RAM and
				// half outside ?
				
				// TODO
			}

		private:

			byte_t* ram;						/// Computer RAM
			const byte_t* rom;			/// Computer ROM chip
			std::size_t ram_size;		/// Computer RAM size
			std::size_t rom_size;		/// Computer ROM size

			std::unique_ptr<ICPU> cpu;	/// Virtual CPU

			std::shared_ptr<IDevice> devices[MAX_N_DEVICES];	/// Devices atached to the virtual computer
			std::shared_ptr<IDevice> sdevices[MAX_N_DEVICES]; /// Devices that run in sync with the VM clock

	};

} // End of namespace vm

#endif // __VCOMPUTER_HPP_
