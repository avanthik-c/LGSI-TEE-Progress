- CPU utilization
- TA execution time
- REE <-> TEE invocation overhead
- Peak heap usage
- Peak stack usage
- CPU cycles
- CPU utilization
- Shared-memory usage
- Session establishment time
- TA loading time
- Binary/code size
- Crash/abort rate




| Metric | What it is | What it tells you | Typical measurement |
|---|---|---|---|
| **CPU utilization** | Percentage of CPU capacity consumed while the TA/program executes. | How heavily the processor is being used. Useful for understanding computational load and system impact. | Measure CPU time relative to elapsed time, or use OS/PMU statistics. |
| **TA execution time** | Time taken by the TA to perform its actual operation, excluding as much external invocation overhead as possible. | The actual computational performance of the program inside the TEE. | Timestamp at the beginning and end of the operation **inside the TA**. |
| **REE ↔ TEE invocation overhead** | Time spent entering the secure world, processing the invocation, and returning to the REE, apart from the actual TA computation. | How much overhead OP-TEE introduces around the application. Important when the TA is invoked frequently. | Timestamp around `TEEC_InvokeCommand()` in the REE and compare with TA-internal execution time. |
| **Peak heap usage** | Maximum amount of dynamically allocated memory used by the TA at one point during execution. | How much dynamic memory the program actually needs at its worst point. Useful for detecting memory pressure. | Instrument allocations/deallocations and track the maximum simultaneous allocation. |
| **Peak stack usage** | Maximum portion of the TA's stack consumed during execution. | Whether the configured TA stack is sufficient and how close execution gets to stack exhaustion. | Use stack high-water-mark/pattern instrumentation or debugger-based stack inspection. |
| **CPU cycles** | Number of processor cycles required for an operation. | Processor-level computational cost. Often more useful than wall-clock time when comparing implementations. | Read ARM PMU/cycle counter before and after the operation. |
| **Shared-memory usage** | Amount of memory used to exchange data between the REE and TEE/TA. | Communication memory requirements and potentially the cost of transferring large inputs/outputs. | Record sizes of `TEEC_SharedMemory`/registered memory buffers. |
| **Session establishment time** | Time required to establish a communication session between the REE client and the TA. | Startup/connection overhead before TA commands can be executed. | Timestamp before and after `TEEC_OpenSession()`. |
| **TA loading time** | Time associated with loading/initializing the TA before it can execute normally. | Startup cost of the TA, including effects such as TA loading and initialization. | Compare first invocation/startup against subsequent invocations, or instrument the loading/initialization path. |
| **Binary/code size** | Static size of the TA executable and its code/data sections. | Storage/firmware footprint of the TA; useful when evaluating resource-constrained systems. | Inspect the TA ELF using tools such as `size`, `readelf`, or equivalent. |
| **Crash/abort rate** | Frequency with which TA executions terminate abnormally. | Reliability and robustness under repeated or stressed execution. | Run the TA many times and count failed invocations, panics/aborts, or unexpected termination. |