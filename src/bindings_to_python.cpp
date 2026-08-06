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
    py::class_<NPETComm>(m, "NPET_comm")
            .def(py::init<>())
            .def("open_NPET_communication", &NPETComm::openCommunication)
            .def("is_NPET_connected", &NPETComm::isResponsive)
            .def("set_NPET_FW_ver", &NPETComm::setFWVer)
            .def("set_frequency", &NPETComm::setFrequency)
            .def("set_baud_rate", &NPETComm::setBaudRate)
            .def("generate_pulses", &NPETComm::generatePulses)
            .def("set_measured_data_format", &NPETComm::setMeasuredDataFormat)
            .def("export_time_constant", &NPETComm::exportTimeConstantRaw)
            .def("clear_time_constant", &NPETComm::clearTimeConstant)
            .def("import_time_constant", &NPETComm::importTimeConstantRaw)
            .def("read_single_measurement", &NPETComm::readSingleMeasurementRaw)
            .def("read_measurements", &NPETComm::readBatchMeasurements);
}
