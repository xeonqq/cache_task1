/*
// File: task_1.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
//
// Author(s): Michiel W. van Tol, Mike Lankamp, Jony Zhang, 
//            Konstantinos Bousias
// Copyright (C) 2005-2009 by Computer Systems Architecture group, 
//                            University of Amsterdam
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
 */

#include "aca2009.h"
#include <systemc.h>
#include <iostream>

using namespace std;

static const int MEM_SIZE = 512;

typedef	struct 
{
	sc_uint<1> index;
	sc_uint<7> tag;
	sc_int<8> data[32]; 
} aca_cache_line;

typedef	struct 
{
	aca_cache_line cache_line[128];
} aca_cache_set; 

typedef	struct
{
	aca_cache_set cache_set[8];
} aca_cache;

SC_MODULE(Cache) 
{

	public:
		aca_cache cache;


		enum Function 
		{
			FUNC_READ,
			FUNC_WRITE
		};

		enum RetCode 
		{
			RET_READ_DONE,
			RET_WRITE_DONE,
		};

		sc_in<bool>     Port_CLK;
		sc_in<Function> Port_Func;
		sc_in<int>      Port_Addr;
		sc_out<RetCode> Port_Done;
		sc_inout_rv<32> Port_Data;
		sc_out<bool> 	Port_Hit;

		SC_CTOR(Cache) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();

			//m_data = new int[MEM_SIZE];
		}

		~Cache() 
		{
			//delete[] m_data;
		}

	private:
		//int* m_data;

		void execute() 
		{
			while (true)
			{
				wait(Port_Func.value_changed_event());

				Function f = Port_Func.read();
				int addr   = Port_Addr.read();
				int data   = 0;
				int index  = 0;
				if (f == FUNC_WRITE) 
				{
					//determine whether a write hit
					index = (addr & 0x0FE0) >> 5;	
					cout << sc_time_stamp() << ": MEM received write" << endl;
					data = Port_Data.read().to_int();
				}
				else
				{
					cout << sc_time_stamp() << ": MEM received read" << endl;
				}

				// This simulates memory read/write delay
				wait(99);

				if (f == FUNC_READ) 
				{
					//Port_Data.write( (addr < MEM_SIZE) ? m_data[addr] : 0 );
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
				else
				{
					if (addr < MEM_SIZE) 
					{
					//	m_data[addr] = data;
					}
					Port_Done.write( RET_WRITE_DONE );
				}
			}
		}
}; 

SC_MODULE(CPU) 
{

	public:
		sc_in<bool>                Port_CLK;
		sc_in<Cache::RetCode>   Port_MemDone;
		sc_out<Cache::Function> Port_MemFunc;
		sc_out<int>                Port_MemAddr;
		sc_inout_rv<32>            Port_MemData;

		SC_CTOR(CPU) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();
		}

	private:
		void execute() 
		{
			TraceFile::Entry    tr_data;
			Cache::Function  f;

			// Loop until end of tracefile
			while(!tracefile_ptr->eof())
			{
				// Get the next action for the processor in the trace
				if(!tracefile_ptr->next(0, tr_data))
				{
					cerr << "Error reading trace for CPU" << endl;
					break;
				}

				// To demonstrate the statistic functions, we generate a 50%
				// probability of a 'hit' or 'miss', and call the statistic
				// functions below
				int j = rand()%2;

				switch(tr_data.type)
				{
					case TraceFile::ENTRY_TYPE_READ:
						f = Cache::FUNC_READ;
						if(j)
							stats_readhit(0);
						else
							stats_readmiss(0);
						break;

					case TraceFile::ENTRY_TYPE_WRITE:
						f = Cache::FUNC_WRITE;
						if(j)
							stats_writehit(0);
						else
							stats_writemiss(0);
						break;

					case TraceFile::ENTRY_TYPE_NOP:
						break;

					default:
						cerr << "Error, got invalid data from Trace" << endl;
						exit(0);
				}

				if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
				{
					Port_MemAddr.write(tr_data.addr);
					Port_MemFunc.write(f);

					if (f == Cache::FUNC_WRITE) 
					{
						cout << sc_time_stamp() << ": CPU sends write" << endl;

						uint32_t data = rand();
						Port_MemData.write(data);
						wait();
						Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
					}
					else
					{
						cout << sc_time_stamp() << ": CPU sends read" << endl;
					}

					wait(Port_MemDone.value_changed_event());

					if (f == Cache::FUNC_READ)
					{
						cout << sc_time_stamp() << ": CPU reads: " << Port_MemData.read() << endl;
					}
				}
				else
				{
					cout << sc_time_stamp() << ": CPU executes NOP" << endl;
				}
				// Advance one cycle in simulated time            
				wait();
			}

			// Finished the Tracefile, now stop the simulation
			sc_stop();
		}
};


int sc_main(int argc, char* argv[])
{
	try
	{
		// Get the tracefile argument and create Tracefile object
		// This function sets tracefile_ptr and num_cpus
		init_tracefile(&argc, &argv);

		// Initialize statistics counters
		stats_init();

		// Instantiate Modules
		Cache mem("main_memory");
		CPU    cpu("cpu");

		// Signals
		sc_buffer<Cache::Function> sigMemFunc;
		sc_buffer<Cache::RetCode>  sigMemDone;
		sc_signal<int>              sigMemAddr;
		sc_signal_rv<32>            sigMemData;

		// The clock that will drive the CPU and Cache
		sc_clock clk;

		// Connecting module ports with signals
		mem.Port_Func(sigMemFunc);
		mem.Port_Addr(sigMemAddr);
		mem.Port_Data(sigMemData);
		mem.Port_Done(sigMemDone);

		cpu.Port_MemFunc(sigMemFunc);
		cpu.Port_MemAddr(sigMemAddr);
		cpu.Port_MemData(sigMemData);
		cpu.Port_MemDone(sigMemDone);

		mem.Port_CLK(clk);
		cpu.Port_CLK(clk);

		cout << "Running (press CTRL+C to interrupt)... " << endl;


		// Start Simulation
		sc_start();

		// Print statistics after simulation finished
		stats_print();
	}

	catch (exception& e)
	{
		cerr << e.what() << endl;
	}

	return 0;
}
