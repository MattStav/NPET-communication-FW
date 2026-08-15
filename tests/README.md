# NPET_comm_FW test suite

This is the test suite for the NPET communication firmware. 
The tests are designed to verify the functionality of the firmware and 
ensure that it behaves as expected under various conditions.

The tests are built using the Google Test framework and
can be run using the CMake build system.

Set `-DBUILD_WORKFLOW_TESTS=ON` in the CMake configuration to enable coverage.

### Workflow Tests
The workflow tests are designed to test the end-to-end functionality of the FW.
For this purpose they require virtual COM port pairs, 
to which the FW can connect and communicate with Virtual Machine NPET.
Third party software such as com0com can be used to create virtual COM port pairs.
If virtual COM port pairs are not available, 
the workflow tests can be disabled in configuration via:
`-DBUILD_WORKFLOW_TESTS=OFF`.