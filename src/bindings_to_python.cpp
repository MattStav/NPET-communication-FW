#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "framework/NPET_comm.h"
#include "git_tag.h"
#include "manual_data.h"

namespace py = pybind11;

///
/// Python library with bindings for the NPET communication FW.
PYBIND11_MODULE(NPET_comm_FW_lib, m) {
    m.doc() = "Python bindings for the NPET communication FW";
    m.attr("__version__") = GIT_TAG;
    m.attr("manual_text") = manual_text;

    // TODO: Add documentation to the function itself and all its input arguments
    // TODO: Add a class init method that sets the FW version
    py::class_<NPET_comm>(m, "NPET_comm")
            .def(py::init<>())
            .def("open_NPET_communication", &NPET_comm::open_communication) // TODO: comport num is at 0 based index
            .def("is_NPET_connected", &NPET_comm::is_responsive)
            .def("set_NPET_FW_ver", &NPET_comm::set_FW_ver)
            .def("set_frequency", &NPET_comm::set_frequency)
            .def("set_baud_rate", &NPET_comm::set_baud_rate)
            .def("generate_pulses", &NPET_comm::generate_pulses)
            .def("set_measured_data_format", &NPET_comm::set_measured_data_format)
            .def("export_time_constant", &NPET_comm::export_time_constant_raw)
            .def("clear_time_constant", &NPET_comm::clear_time_constant)
            .def("import_time_constant", &NPET_comm::import_time_constant_raw)
            .def("read_single_measurement", &NPET_comm::read_single_measurement_raw)
            .def("read_measurements", &NPET_comm::read_batch_measurements);
}
