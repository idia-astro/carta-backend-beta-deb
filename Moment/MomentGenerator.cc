#include "MomentGenerator.h"

using namespace carta;

using IM = ImageMoments<casacore::Float>;

MomentGenerator::MomentGenerator(const casacore::String& filename, casacore::ImageInterface<float>* image, int spectral_axis,
    int stokes_axis, MomentProgressCallback progress_callback)
    : _filename(filename),
      _image(image),
      _spectral_axis(spectral_axis),
      _stokes_axis(stokes_axis),
      _sub_image(nullptr),
      _image_moments(nullptr),
      _success(false),
      _progress_callback(progress_callback),
      _moments_calc_count(0) {
    SetMomentTypeMaps();
}

std::vector<CollapseResult> MomentGenerator::CalculateMoments(int file_id, const casacore::ImageRegion& image_region,
    const CARTA::MomentRequest& moment_request, CARTA::MomentResponse& moment_response) {
    _success = false;

    // Collapse results
    std::vector<CollapseResult> collapse_results;

    // Set moment axis
    SetMomentAxis(moment_request);

    // Set moment types
    SetMomentTypes(moment_request);

    // Set pixel range
    SetPixelRange(moment_request);

    // Reset an ImageMoments
    ResetImageMoments(image_region);

    // Calculate moments
    try {
        if (!_image_moments->setMoments(_moments)) {
            _error_msg = _image_moments->errorMessage();
        } else {
            if (!_image_moments->setMomentAxis(_axis)) {
                _error_msg = _image_moments->errorMessage();
            } else {
                casacore::Bool do_temp = true;
                casacore::Bool remove_axis = false;
                casacore::String out_file = GetOutputFileName();
                std::size_t found = out_file.find_last_of("/");
                std::string file_base_name = out_file.substr(found + 1);
                try {
                    _image_moments->setInExCludeRange(_include_pix, _exclude_pix);

                    // Start the timer
                    _start_time = std::chrono::high_resolution_clock::now();

                    // Reset the first progress report
                    _first_report = false;

                    // Do calculations and save collapse results in the memory
                    auto result_images = _image_moments->createMoments(do_temp, out_file, remove_axis);

                    for (int i = 0; i < result_images.size(); ++i) {
                        // Set temp moment file name
                        std::string moment_suffix = GetMomentSuffix(_moments[i]);
                        std::string out_file_name = file_base_name + "." + moment_suffix;

                        // Set a temp moment file Id. Todo: find another better way to assign the temp file Id
                        int moment_type = _moments[i];
                        int moment_file_id = (file_id + 1) * OUTPUT_ID_MULTIPLIER + moment_type;

                        // Fill results
                        std::shared_ptr<casacore::ImageInterface<casacore::Float>> moment_image =
                            dynamic_pointer_cast<casacore::ImageInterface<casacore::Float>>(result_images[i]);
                        collapse_results.push_back(CollapseResult(moment_file_id, out_file_name, moment_image));
                    }
                    _success = true;
                } catch (const AipsError& x) {
                    _error_msg = x.getMesg();
                }
            }
        }
    } catch (AipsError& error) {
        _error_msg = error.getLastMessage();
    }

    // Set is the moment calculation successful or not
    moment_response.set_success(IsSuccess());

    // Get error message if any
    moment_response.set_message(GetErrorMessage());

    return collapse_results;
}

void MomentGenerator::StopCalculation() {
    if (_image_moments) {
        _image_moments->StopCalculation();
    }
}

void MomentGenerator::SetMomentAxis(const CARTA::MomentRequest& moment_request) {
    if (moment_request.axis() == CARTA::MomentAxis::SPECTRAL) {
        _axis = _spectral_axis;
    } else if (moment_request.axis() == CARTA::MomentAxis::STOKES) {
        _axis = _stokes_axis;
    } else {
        std::cerr << "Do not support the moment axis: " << moment_request.axis() << std::endl;
    }
}

void MomentGenerator::SetMomentTypes(const CARTA::MomentRequest& moment_request) {
    int moments_size(moment_request.moments_size());
    _moments.resize(moments_size);
    for (int i = 0; i < moments_size; ++i) {
        _moments[i] = GetMomentMode(moment_request.moments(i));
    }
}

void MomentGenerator::SetPixelRange(const CARTA::MomentRequest& moment_request) {
    // Set include or exclude pixel range
    CARTA::MomentMask moment_mask = moment_request.mask();
    float pixel_min(moment_request.pixel_range().min());
    float pixel_max(moment_request.pixel_range().max());

    if (pixel_max < pixel_min) {
        float tmp = pixel_max;
        pixel_max = pixel_min;
        pixel_min = tmp;
    }

    if (moment_mask == CARTA::MomentMask::Include) {
        _include_pix.resize(2);
        _include_pix[0] = pixel_min;
        _include_pix[1] = pixel_max;
        // Clear the exclusive array
        _exclude_pix.resize(0);
    } else if (moment_mask == CARTA::MomentMask::Exclude) {
        _exclude_pix.resize(2);
        _exclude_pix[0] = pixel_min;
        _exclude_pix[1] = pixel_max;
        // Clear the inclusive array
        _include_pix.resize(0);
    } else {
        // Clear inclusive and exclusive array
        _include_pix.resize(0);
        _exclude_pix.resize(0);
    }
}

void MomentGenerator::ResetImageMoments(const casacore::ImageRegion& image_region) {
    // Reset the sub-image
    _sub_image.reset(new casacore::SubImage<casacore::Float>(*_image, image_region));

    casacore::LogOrigin log("MomentGenerator", "MomentGenerator", WHERE);
    casacore::LogIO os(log);

    // Make an ImageMoments object (and overwrite the output file if it already exists)
    _image_moments.reset(new IM(casacore::SubImage<casacore::Float>(*_sub_image), os, true));

    // Set moment calculation progress monitor
    _image_moments->SetProgressMonitor(this);
}

int MomentGenerator::GetMomentMode(CARTA::Moment moment) {
    if (_moment_map.count(moment)) {
        return _moment_map[moment];
    } else {
        std::cerr << "Unknown moment mode: " << moment << std::endl;
        return -1;
    }
}

casacore::String MomentGenerator::GetMomentSuffix(casacore::Int moment) {
    auto moment_type = static_cast<IM::MomentTypes>(moment);
    if (_moment_suffix_map.count(moment_type)) {
        return _moment_suffix_map[moment_type];
    } else {
        std::cerr << "Unknown moment mode: " << moment << std::endl;
        return "";
    }
}

casacore::String MomentGenerator::GetOutputFileName() {
    // Store moment images in a temporary folder
    casacore::String result(_filename);
    result += ".moment";
    size_t found = result.find_last_of("/");
    if (found) {
        result.replace(0, found, "");
    }
    return result;
}

bool MomentGenerator::IsSuccess() const {
    return _success;
}

casacore::String MomentGenerator::GetErrorMessage() const {
    return _error_msg;
}

void MomentGenerator::setStepCount(int count) {
    // Initialize the progress parameters
    _total_steps = count;
    _progress = 0.0;
    _pre_progress = 0.0;
}

void MomentGenerator::setStepsCompleted(int count) {
    _progress = (float)count / _total_steps;
    if (_progress > MOMENT_COMPLETE) {
        _progress = MOMENT_COMPLETE;
    }

    if (!_first_report) {
        auto current_time = std::chrono::high_resolution_clock::now();
        auto dt = std::chrono::duration<double, std::milli>(current_time - _start_time).count();
        if (dt >= REPORT_FIRST_PROGRESS_AFTER_MILLI_SECS) {
            _progress_callback(_progress);
            _first_report = true;
        }
    }

    // Update the progress report every percent
    if ((_progress - _pre_progress) >= REPORT_PROGRESS_EVERY_FACTOR) {
        _progress_callback(_progress);
        _pre_progress = _progress;
        if (!_first_report) {
            _first_report = true;
        }
    }
}

void MomentGenerator::done() {}

void MomentGenerator::IncreaseMomentsCalcCount() {
    _moments_calc_count++;
}

void MomentGenerator::DecreaseMomentsCalcCount() {
    _moments_calc_count--;
}

void MomentGenerator::DisconnectCalled() {
    StopCalculation();                // Call to stop the moments calculation
    while (_moments_calc_count > 0) { // Wait the moments calculation finished
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

inline void MomentGenerator::SetMomentTypeMaps() {
    _moment_map[CARTA::Moment::MEAN_OF_THE_SPECTRUM] = IM::AVERAGE;
    _moment_map[CARTA::Moment::INTEGRATED_OF_THE_SPECTRUM] = IM::INTEGRATED;
    _moment_map[CARTA::Moment::INTENSITY_WEIGHTED_COORD] = IM::WEIGHTED_MEAN_COORDINATE;
    _moment_map[CARTA::Moment::INTENSITY_WEIGHTED_DISPERSION_OF_THE_COORD] = IM::WEIGHTED_DISPERSION_COORDINATE;
    _moment_map[CARTA::Moment::MEDIAN_OF_THE_SPECTRUM] = IM::MEDIAN;
    _moment_map[CARTA::Moment::MEDIAN_COORDINATE] = IM::MEDIAN_COORDINATE;
    _moment_map[CARTA::Moment::STD_ABOUT_THE_MEAN_OF_THE_SPECTRUM] = IM::STANDARD_DEVIATION;
    _moment_map[CARTA::Moment::RMS_OF_THE_SPECTRUM] = IM::RMS;
    _moment_map[CARTA::Moment::ABS_MEAN_DEVIATION_OF_THE_SPECTRUM] = IM::ABS_MEAN_DEVIATION;
    _moment_map[CARTA::Moment::MAX_OF_THE_SPECTRUM] = IM::MAXIMUM;
    _moment_map[CARTA::Moment::COORD_OF_THE_MAX_OF_THE_SPECTRUM] = IM::MAXIMUM_COORDINATE;
    _moment_map[CARTA::Moment::MIN_OF_THE_SPECTRUM] = IM::MINIMUM;
    _moment_map[CARTA::Moment::COORD_OF_THE_MIN_OF_THE_SPECTRUM] = IM::MINIMUM_COORDINATE;

    _moment_suffix_map[IM::AVERAGE] = "average";
    _moment_suffix_map[IM::INTEGRATED] = "integrated";
    _moment_suffix_map[IM::WEIGHTED_MEAN_COORDINATE] = "weighted_coord";
    _moment_suffix_map[IM::WEIGHTED_DISPERSION_COORDINATE] = "weighted_dispersion_coord";
    _moment_suffix_map[IM::MEDIAN] = "median";
    _moment_suffix_map[IM::MEDIAN_COORDINATE] = "median_coord";
    _moment_suffix_map[IM::STANDARD_DEVIATION] = "standard_deviation";
    _moment_suffix_map[IM::RMS] = "rms";
    _moment_suffix_map[IM::ABS_MEAN_DEVIATION] = "abs_mean_dev";
    _moment_suffix_map[IM::MAXIMUM] = "maximum";
    _moment_suffix_map[IM::MAXIMUM_COORDINATE] = "maximum_coord";
    _moment_suffix_map[IM::MINIMUM] = "minimum";
    _moment_suffix_map[IM::MINIMUM_COORDINATE] = "minimum_coord";
}