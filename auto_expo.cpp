// CPP STANDARD: c++17
#include <iostream>
#include <algorithm>
#include <map>
#include <ctime>
#include <cmath>
#include <iomanip>
#include <utility>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <climits>
#include <memory>
#include <sstream>
#include <queue>
#include <deque>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <tuple>
#include <chrono>
#include <thread>
#include <csignal>
#include <system_error>
#include <string>
#include <filesystem>
#include <charconv>

#include <errno.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <fitsio.h>
#include <gps.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
//#define STRIP_FLAG_HELP 1
#include <gflags/gflags.h>

#include "App.h" // from https://github.com/uNetworking/uWebSockets
#include "ASICamera2.h"

namespace fs = std::filesystem;

static bool ValidateExpoMs(const char *flagname, double expoms) {
    if (0 < expoms && expoms < 1000*1000) {
        return true;
    }
    std::cerr << flagname << " out of range! (should be 0~1000000)" << std::endl;
    return false;
}

static bool ValidateGain(const char *flagname, int32_t gain) {
    if (0 <= gain && gain <= 300) {
        return true;
    }
    std::cerr << flagname << " out of range! (should be 0~300" << std::endl;
    return false;
}

DEFINE_int32(bin, 2, "Camera BIN (1, 2, 3, 4)");
DEFINE_string(img_type, "RAW16", "asi camera image type, currently only support RAW8 and RAW16");
DEFINE_int32(gain, 0, "Gain, between 0~300");
DEFINE_string(output, "/tmp/data/", "output dir");
DEFINE_string(prefix, "debris_", "output filename prefix");
DEFINE_bool(cooler, true, "CoolerON (--nocooler for CoolerOFF");
DEFINE_int32(cool_temp, -30, "Cooler Target Temperature");
DEFINE_bool(mono_bin, true, "mono bin (--nomonobin for no monobin)");
DEFINE_int32(log_level, spdlog::level::info, "log level, 0~5: trace,debug,info,warn,err,critical");
DEFINE_uint64(expo_count, 0, "exposure loop count, 0 is infinity");
DEFINE_double(expo_ms, 1000, "exposure time in milliseconds");
DEFINE_bool(wait_cooling, false, "wait for cooling for some while");
DEFINE_bool(keep_running, true, "still keep the program running when all expo finished");

DEFINE_validator(gain, &ValidateGain);
DEFINE_validator(expo_ms, &ValidateExpoMs);

#if 0
static unsigned long GetTickCount() {
#ifdef __linux__
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC,&ts);
   return (ts.tv_sec*1000 + ts.tv_nsec/(1000*1000));
#else
#error "ONLY support Linux yet"
#endif
//	struct timeval  now;
//    gettimeofday(&now, NULL);
//    unsigned long ul_ms = now.tv_usec/1000 + now.tv_sec*1000;
//    return ul_ms;
}
#endif

/**
 * ## Config ##
 *  	(camera setup)
 *   - bin mode
 *   - color bin
 *   - img type (ASI_IMG_TYPE)
 *   - cooler
 *   - cooler temp
 *   - gain
 *   - expo
 *   - interval
 *   - mirror
 *   - select model
 *      (output)
 *   - output filepath
 *   - output filetype
 *   - output name pattern
 *   - log level
 *   - debug
 */

/**
 * ## Attributes ##
 *  [format] :key: :value: :desc:
 */

/**
 * ## Process ##
 *   - load config
 *   - access gpsd
 *   - find cameras
 *   - init cooler
 *   - setup 
 *   - init RPC server
 *   - start expo
 */

// ASI_ERR_CHECK

//const std::string output_format[] = {
//    "hex",
//    "bin",
//    "fits",
//};

const std::map<ASI_BAYER_PATTERN, const std::string> _BayerPattern = {
    {ASI_BAYER_PATTERN::ASI_BAYER_RG, "ASI_BAYER_RG"}, 
    {ASI_BAYER_PATTERN::ASI_BAYER_BG, "ASI_BAYER_BG"},
    {ASI_BAYER_PATTERN::ASI_BAYER_GB, "ASI_BAYER_GB"}, 
    {ASI_BAYER_PATTERN::ASI_BAYER_GR, "ASI_BAYER_GR"},
};

const std::map<ASI_IMG_TYPE, const std::string> _ImgType = {
    {ASI_IMG_TYPE::ASI_IMG_RAW8, "ASI_IMG_RAW8"},
    {ASI_IMG_TYPE::ASI_IMG_RAW16, "ASI_IMG_RAW16"},
    {ASI_IMG_TYPE::ASI_IMG_RGB24, "ASI_IMG_RGB24"},
    {ASI_IMG_TYPE::ASI_IMG_Y8, "ASI_IMG_Y8"},
    {ASI_IMG_TYPE::ASI_IMG_END, "ASI_IMG_END"},
};

const std::map<ASI_CONTROL_TYPE, const std::string> _CtrlType = {
    {ASI_GAIN, "ASI_GAIN"},
    {ASI_EXPOSURE, "ASI_EXPOSURE"},
    {ASI_COOLER_ON, "ASI_COOLER"},
    {ASI_TARGET_TEMP, "ASI_TARGET_TEMP"},
    {ASI_GAMMA, "ASI_GAMMA"},
    {ASI_MONO_BIN, "ASI_MONO_BIN"},
    {ASI_HIGH_SPEED_MODE, "ASI_HIGH_SPEED_MODE"},
    {ASI_BANDWIDTHOVERLOAD, "ASI_BANDWIDTHOVERLOAD"},
    {ASI_FLIP, "ASI_FLIP"},
};

void print_asicam_info(const ASI_CAMERA_INFO *info) {
    std::cout << "camName: " << info->Name << ",\t";
    std::cout << "camId: " << info->CameraID << ",\t";
    std::cout << "maxHeight: " << info->MaxHeight << ",\t";
    std::cout << "maxWidth: " << info->MaxWidth << ",\t";
    std::cout << std::endl;
    std::cout << "IsCoolerCam: " << info->IsColorCam << ",\t";
    std::cout << "PixelSize: " << info->PixelSize << ",\t";
    std::cout << "BitDepth: " << info->BitDepth << ",\t";
    std::cout << "ElecPerADU: " << info->ElecPerADU << ",\t";
    std::cout << "IsUSB3Host: " << info->IsUSB3Host << ",\t";
    std::cout << "IsUSB3Camera: " << info->IsUSB3Camera << ",\t";
    std::cout << std::endl;
    std::cout << "BayerPattern: " << _BayerPattern.at(info->BayerPattern) << ",\t";
    std::cout << "hasMechanicalShutte: " << info->MechanicalShutter << ",\t";
    std::cout << "IsTriggerCam: " << info->IsTriggerCam << ",\t";
    std::cout << std::endl;
}

void print_asicam_control_caps(const ASI_CONTROL_CAPS *caps) {
    std::cout << "capsName: " << caps->Name << ",\t";
    std::cout << "capsDesc: " << caps->Description << ",\t";
    std::cout << "MaxValue: " << caps->MaxValue << ",\t";
    std::cout << "MinValue: " << caps->MinValue << ",\t";
    std::cout << std::endl;
    std::cout << "DefaultValue: " << caps->DefaultValue << ",\t";
    std::cout << "IsAutoSupported: " << caps->IsAutoSupported << ",\t";
    std::cout << "IsWritable: " << caps->IsWritable << ",\t";
    std::cout << "ControlType: " << caps->ControlType << ",\t";
    std::cout << std::endl;
}

class CircularBuffer {
    public:
        CircularBuffer(unsigned long buf_len, int buf_cnt = 20): _buf_len(buf_len), _buf_cnt(buf_cnt) {
            _buf = new uint8_t *[_buf_cnt];
            for (int i = 0; i < _buf_cnt; i++) {
                _buf[i] = new uint8_t[_buf_len];
                memset(_buf[i], 0, _buf_len);
            }
        }
        ~CircularBuffer() {
            for (int i = 0; i < _buf_cnt; i++) {
                delete [] _buf[i];
            }
            delete [] _buf;
            _buf_len = 0;
            _buf_cnt = 0;
            _buf = nullptr;
        }
        auto gen_buff() {
            uint8_t *cur_buf = _buf[_buf_cur_pos];
            _buf_cur_pos += 1;
            _buf_cur_pos %= _buf_cnt;
            return std::make_tuple(cur_buf, _buf_len);
        }
    private:
        uint8_t **_buf = nullptr;
        unsigned long _buf_len = 0;
        int _buf_cnt = 0;
        int _buf_cur_pos = 0;
};

// TODO Build Class with Key, Val, Desc. Could cast
// At present just save internally often changed value
typedef struct {
    unsigned long expo_cnt;
    std::chrono::system_clock::time_point tim_expo_start;
    std::chrono::steady_clock::duration tik_expo_stop;
    std::chrono::steady_clock::duration tik_acq_end;
//    int imgHeight;
//    int imgWidth;
//    int imgBin;
    float real_temp;
    // ASI_IMG_TYPE imgType;
    // read from gpsd
    double gps_latitude = 0;
    double gps_longitude = 0;
    double gps_altitude = 0;
    int gps_status = 0;
    int gps_fix_mode = 0;
    int gps_satellites = 0;
    double gps_fix_time = 0;
    double gps_online = 0;
    int gps_connected = 0;
} ImgMeta;

class GPSInfo {
    public:
        // TODO singleton mode
        static std::shared_ptr<GPSInfo> gpsInfo;
        GPSInfo () {
            gps_connected = 0;
            isloop = 1;
            th_gps = std::make_unique<std::thread>(&GPSInfo::loop, this);
        }
        ~GPSInfo() {
            logger->warn("release gps info");
            stop();
        }
        void stop() {
            isloop = 0;
            if (th_gps) {
                th_gps->join();
                th_gps = nullptr;
            }
            gps_connected = 0;
        }
        void loop() {
            int ret = 0;
            while (isloop) {
                if (!gps_connected) {
                    ret = gps_open("127.0.0.1", "2947", &gps_data);
                    if (ret != 0) {
                        logger->error("open gpsd failed {}", gps_errstr(ret));
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        continue;
                    }
                    gps_connected = 1;
                    gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);
                }
                if (gps_waiting (&gps_data, 500)) {
                    if (gps_read (&gps_data) == -1) {
                        logger->error("read gps failed");
                        gps_stream(&gps_data, WATCH_DISABLE, NULL);
                        gps_close (&gps_data);
                        gps_connected = 0;
                        continue;
                    }
                }
            }
        }
        int get_connected() const {
            return gps_connected;
        }
        auto get_lat_lon_alt() const {
            auto lat = gps_data.fix.latitude;
            auto lon = gps_data.fix.longitude;
            auto alt = gps_data.fix.altitude;
            return std::tuple<double, double, double>({
                std::isnan(lat) ? -999 : lat,
                std::isnan(lon) ? -999 : lon,
                std::isnan(alt) ? -999 : alt,
            });
        }
        auto get_satellites() const {
            return gps_data.satellites_visible;
        }
        auto get_time() const {
            auto time = gps_data.fix.time;
            auto online = gps_data.online;
            return std::tuple<double, double>({
                std::isnan(time) ? 0 : time, 
                std::isnan(online) ? 0 : online,
            });
        }
        auto get_state() const {
            return std::tuple<int, int>({gps_data.fix.mode, gps_data.status});
        }
    private:
        volatile int isloop = 0;
        std::unique_ptr<std::thread> th_gps = nullptr;
        struct gps_data_t gps_data;
        std::atomic_int gps_connected;
        std::shared_ptr<spdlog::logger> logger = spdlog::stdout_color_mt("GPS");
};
std::shared_ptr<GPSInfo> GPSInfo::gpsInfo = nullptr;

class ASICamHandler {
    public:
        ASICamHandler (std::shared_ptr<spdlog::logger> _logger=nullptr) : logger(_logger){
            if (_logger == nullptr) {
                std::ostringstream ost;
                ost << "ASICAM" << _log_cnt++;
                logger = spdlog::stdout_color_mt(ost.str());
            }
        }
        ASICamHandler (const ASICamHandler & p) = delete;
        ~ASICamHandler() {
            std::thread tid([]() {
                // ugly force shutdown (to avoid block);
                std::this_thread::sleep_for(std::chrono::seconds(10));
                abort();
            });
            tid.detach();
            th_expo_stop = 1;
            if (th_expo) {
                th_expo->join();
            }
        }
#if 0
        // TODO consider thread safety ?
        // TODO not work (If get exp very freq)
        void traceError(int err) {
            err_queue.push_back(err);
            if (err_queue.size() > 50) {
                err_queue.pop_front();
            }
            auto success_cnt = std::count(err_queue.begin(), err_queue.end(), ASI_SUCCESS);
            //if ((float)success_cnt / err_queue.size() < 0.2) {
            if (err_queue.size() - success_cnt > 10) {
                logger->critical("[cam-{}] Often fails {}/{}, consider restart please", info.CameraID, err_queue.size() - success_cnt, err_queue.size());
                th_expo_stop = 1;
                alarm(30);
            }
        }
#else
        void traceError(int err) {
            static int err_cnt;
            static auto last_err = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();

            if (err == ASI_SUCCESS) return;

            if (std::chrono::duration_cast<std::chrono::minutes>(now - last_err).count() > 30) {
                // no new error in last 30 minutes
                err_cnt = 0; // reset counter

            }
            err_cnt++;
            if (err_cnt >= 16) {
                logger->critical("[cam-{}] too many error, consider restart", info.CameraID);
                th_expo_stop = 1;
                alarm(20);
            }
            last_err = now;
        }
#endif
        std::string prodId;
        void setInfo() {}
        void setInfo(ASI_CAMERA_INFO _info) {
            info = std::move(_info);
        }
        void setCtrlCaps(ASI_CONTROL_CAPS _caps) {
            ctrlMap[_caps.ControlType] = _caps;
        }
//        void setCtrlCaps(ASI_CONTROL_TYPE _type, ASI_CONTROL_CAPS _caps) {
//            ctrlMap[_type] = _caps;
//        }
        void saveCtrlVal(ASI_CONTROL_TYPE _type, long val, ASI_BOOL _auto) {
            // TODO consistency check
            ctrlVal[_type] = val;
            ctrlValAuto[_type] = _auto;
        }
        auto setCtrlVal(ASI_CONTROL_TYPE _type, long val, ASI_BOOL _auto) {
            logger->info("[cam-{}] setCtrl {}, val: {}, auto: {}", info.CameraID, _CtrlType.at(_type), val, _auto);
            auto res = ASISetControlValue(info.CameraID, _type, val, _auto);
            if (res) {
                logger->error("[cam-{}] setCtrl {} failed: {}", info.CameraID, _CtrlType.at(_type), res);
            }
            else {
                assert(ASIGetControlValue(info.CameraID, _type, &val, &_auto) == ASI_SUCCESS);
                saveCtrlVal(_type, val, _auto);
            }

            // post setup
            // XXX Ugly sync expo
            if (_type == ASI_EXPOSURE &&
                    val != (long)(expo_ms * 1000)) {
                expo_ms = val / 1000.0;
            }
            return res;
        }
        auto setExpoMilliSec(double _expo_ms) {
            if (_expo_ms < 0 || _expo_ms > 60*1000) {
                logger->error("invalid expo_ms setup {}", _expo_ms);
                return ASI_ERROR_GENERAL_ERROR;
            }
            expo_ms = _expo_ms;
            long expo_us = (long)(expo_ms * 1000);
            return setCtrlVal(ASI_EXPOSURE, expo_us, ASI_FALSE);
        }
        auto setCoolerTemp(int32_t temp) {
            auto caps = ctrlMap[ASI_TARGET_TEMP];
            if (temp >= caps.MaxValue && temp <= caps.MinValue) {
                logger->error("Invalid cooling target temperature: {}. Should between {}~{}", temp, caps.MinValue, caps.MaxValue);
                return ASI_ERROR_GENERAL_ERROR;
            }
            auto ret = setCtrlVal(ASI_TARGET_TEMP, temp, ASI_FALSE);
            cool_temp = ctrlVal[ASI_TARGET_TEMP];
            return ret;
        }
        // TODO consider start pos
        auto setROIFormat(int bin, ASI_IMG_TYPE _type) {
            // XXX At present just assume to use the whole image
            int camId = info.CameraID;
            int roiWidth = info.MaxWidth / bin;
            int roiHeight = info.MaxHeight / bin;
            auto res = ASISetROIFormat(camId, roiWidth, roiHeight, bin, _type);
            if (res) {
                logger->error("[cam-{}] set roi failed: {}", res);
                abort();
            }
            assert(ASIGetROIFormat(camId, &roiWidth, &roiHeight, &bin, &_type) == ASI_SUCCESS);
            curBin = bin;
            curHeight = roiHeight;
            curWidth = roiWidth;
            imgType = _type;
            logger->info("[cam-{}] current roi| width: {}, height: {}, bin: {}, imgtype: {}", camId, curWidth, curHeight, curBin, _ImgType.at(imgType));
            return res;
        }
        auto camId() const {
            return info.CameraID;
        }
        auto camName() const {
            return info.Name;
        }
        auto* pInfo() {
            return &(info);
        }
        auto maxHeight() const {
            return info.MaxHeight;
        }
        auto maxWidth() const {
            return info.MaxWidth;
        }

        void addGPSinfo(std::shared_ptr<ImgMeta> meta, std::shared_ptr<GPSInfo> gpsinfo) {
            std::tie(meta->gps_latitude, meta->gps_longitude, meta->gps_altitude) = gpsinfo->get_lat_lon_alt();
            meta->gps_satellites = gpsinfo->get_satellites();
            std::tie(meta->gps_fix_time, meta->gps_online) = gpsinfo->get_time();
            std::tie(meta->gps_fix_mode, meta->gps_status) = gpsinfo->get_state();
            meta->gps_connected = gpsinfo->get_connected();
        }

        void reset_expo_count(unsigned long loop_cnt) {
            logger->warn("[cam-{}] reset expo target loop count to {}", camId(), loop_cnt);
            this->loop_cnt = loop_cnt;
            this->expo_cnt = 0;
        }
        void stop_expo_count() {
            logger->warn("[cam-{}] stop expo", camId());
            ASIStopExposure(camId());
            this->expo_cnt = ULONG_MAX-1;
            this->loop_cnt = 1;
        }

        void expo_loop_start(unsigned long loop_cnt = 0);
        void expo_wait_cooling();
        void data_saving_start();
        void monitor_loop();
        int monitor_wait() {
            if (th_mon->joinable()) {
                th_mon->join();
            }
            th_mon = nullptr;
            return 0;
        }
        int expo_loop_wait() {
            if (th_expo->joinable()) {
                th_expo->join();
            }
            // TODO thread safety
            th_expo = nullptr;
            th_expo_stop = 1;
            return 0;
        }
        int data_saving_wait() {
            if (th_data->joinable()) {
                th_data->join();
            }
            th_data = nullptr;
            return 0;
        }
        void create_circular_buff(unsigned long buf_len, int buf_cnt=20) {
            assert(buf_cnt >= 1);
            unsigned char **circular_buf;
            circular_buf = new unsigned char *[buf_cnt];
            for (int i = 0; i < buf_cnt; i++) {
                circular_buf[i] = new unsigned char[buf_len];
            }
        }
        void save_data(uint8_t *buf, unsigned long len, std::shared_ptr<ImgMeta> meta);
        void stop() {
            th_expo_stop = 1;
        }
        void wait_all_threads() {
            expo_loop_wait();
            data_saving_wait();
            monitor_wait();
        }

        // TODO set private
        int curHeight = 0;
        int curWidth = 0;
        int curBin = 0;
        ASI_IMG_TYPE imgType = ASI_IMG_END;
        int32_t cool_temp = INT_MIN;
    private:
        ASI_CAMERA_INFO info = {};
        std::map<ASI_CONTROL_TYPE, ASI_CONTROL_CAPS> ctrlMap;
        std::map<ASI_CONTROL_TYPE, long> ctrlVal;
        std::map<ASI_CONTROL_TYPE, ASI_BOOL> ctrlValAuto;

        unsigned long loop_cnt = 0;
        unsigned long expo_cnt = 0;
        static int _log_cnt;
        std::shared_ptr<spdlog::logger> logger;

        std::deque<int> err_queue;

        double expo_ms = 100;
        float real_temp = INT_MAX;

        std::unique_ptr<CircularBuffer> cir_buf = nullptr;
        std::unordered_set<void *> buf_to_proc;
        using ImgDataType = std::tuple<uint8_t *, unsigned long, std::shared_ptr<ImgMeta>>; // pBuf, buflen, metadata
        std::queue<ImgDataType> data_q;
        std::mutex mtx_data;
        std::unique_ptr<std::thread> th_expo = nullptr;
        std::unique_ptr<std::thread> th_mon = nullptr;
        std::unique_ptr<std::thread> th_data = nullptr;

        // TODO atomic
        std::atomic_int th_expo_stop = 0;

};

int ASICamHandler::_log_cnt = 0;

const std::map<ASI_IMG_TYPE, int> ASIImg2Fits = {
    {ASI_IMG_RAW16, USHORT_IMG},
    {ASI_IMG_RAW8, BYTE_IMG},
};

const std::map<int, int> FitsTypeConv = {
    {ASI_IMG_RAW16, TUSHORT},
    {ASI_IMG_RAW8, TBYTE},
};

void ASICamHandler::save_data(uint8_t *buf, unsigned long len, std::shared_ptr<ImgMeta> meta) {
    (void)len;
    logger->debug("saving data {}", meta->expo_cnt);
//    static unsigned long cnt;

    // @ref https://stackoverflow.com/questions/24686846/get-current-time-in-milliseconds-or-hhmmssmmm-format
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(meta->tim_expo_start.time_since_epoch()) % 1000;
    time_t rawtime = std::chrono::system_clock::to_time_t(meta->tim_expo_start);
//    struct tm* utctm = gmtime(&rawtime);
    std::tm *utctm = std::gmtime(&rawtime);
    std::ostringstream ostim;
    ostim << std::put_time(utctm, "%F %T");
    ostim << '.' << std::setfill('0') << std::setw(3) << ms.count();
//    static char timestrbuf[50];
//    strftime(timestrbuf, sizeof(timestrbuf), "%F %T.", utctm);


    // fileformat: <output>/<year>/<month>/<day>/<prefix>_%Y_%m_%d_%H_%M_%S_%ms.fits.fz
    std::ostringstream diros;
    diros << FLAGS_output \
        << "/" << std::put_time(utctm, "%Y") \
       << "/" << std::put_time(utctm, "%m%d") \
       << "/" << std::put_time(utctm, "%H");
    fs::path output = diros.str();
    if (!fs::exists(output)) {
        std::error_code ec;
//        ec.clear();
        if (!fs::create_directories(output, ec)) {
            logger->critical("create dir {} failed: {}", output.c_str(), ec.message());
            abort();
        }
    }

    std::ostringstream fname;
    fname << "!" << output.c_str() << "/" << FLAGS_prefix;
//    fname << "!" << "output-" << cnt++ << ".fits.fz[compress]";
    fname << prodId << "_";
    fname << std::put_time(utctm, "%Y%m%d_%H%M%S_");
    fname << std::setfill('0') << std::setw(3) << ms.count();
    fname << ".fits.fz[compress]";

    logger->warn("save expo {} as {}", meta->expo_cnt, fname.str());

    fitsfile *fptr;
    int status = 0;
    long fpixel = 1, naxis = 2, nelements = 0;
    // long naxes[2] = {meta->imgWidth, meta->imgHeight};
    long naxes[2] = {curWidth, curHeight};
    nelements = naxes[0] * naxes[1];

    fits_create_file(&fptr, fname.str().c_str(), &status);
    // fits_create_img(fptr, ASIImg2Fits.at(meta->imgType), naxis, naxes, &status);
    fits_create_img(fptr, ASIImg2Fits.at(imgType), naxis, naxes, &status);
    fits_write_date(fptr, &status);
//    char input[] = "test";
//    fits_write_key(fptr, TSTRING, "TEST", input, "test comment", &status);

    fits_write_key(fptr, TSTRING, "DATE-OBS", const_cast<char *>(ostim.str().c_str()), "UTC start time of exposure", &status);
    fits_write_key(fptr, TSTRING, "CAM_NAME", info.Name, "Camera name", &status);
    fits_write_key(fptr, TDOUBLE, "EXPO_MS", &expo_ms, "Exposure time in MilliSeconds", &status);
    fits_write_key(fptr, TINT, "COOLTEMP", &cool_temp, "Target cooling temperature", &status);
    fits_write_key(fptr, TLONG, "COOLER", &(ctrlVal[ASI_COOLER_ON]), "CoolerON: 1,  CoolerOFF: 0", &status);
    fits_write_key(fptr, TFLOAT, "TEMPERAT", &(meta->real_temp), "Camera temperature", &status);
    fits_write_key(fptr, TSTRING, "IMG_TYPE", const_cast<char *>(_ImgType.at(imgType).c_str()), "ASI img type", &status);
    fits_write_key(fptr, TLONG, "GAIN", &(ctrlVal[ASI_GAIN]), "The ratio of output to input", &status);
    fits_write_key(fptr, TLONG, "MONO_BIN", &(ctrlVal[ASI_MONO_BIN]), ctrlMap[ASI_MONO_BIN].Description, &status);
    fits_write_key(fptr, TINT, "BINNING", &curBin, "camera bin", &status);
    fits_write_key(fptr, TINT, "CAM_WIDTH", &(info.MaxWidth), "camera max width", &status);
    fits_write_key(fptr, TINT, "CAM_HEIGHT", &(info.MaxHeight), "camera max height", &status);
    auto _expo_stop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(meta->tik_expo_stop).count();
    fits_write_key(fptr, TULONG, "EXPO_TIM", &_expo_stop_ms, "time used for expo end in milliseconds", &status);
    auto _expo_total_ms = std::chrono::duration_cast<std::chrono::milliseconds>(meta->tik_acq_end).count();
    fits_write_key(fptr, TULONG, "ACQ_TIM", &_expo_total_ms, "time used for total expo and acquire data in ms", &status);
    // add gps related keys
    fits_write_key(fptr, TINT, "GPSDCONN", &(meta->gps_connected), "GPSD connection state, 1 for connected", &status);
    fits_write_key(fptr, TDOUBLE, "GPS_LAT", &(meta->gps_latitude), "Latitude from GPS", &status);
    fits_write_key(fptr, TDOUBLE, "GPS_LON", &(meta->gps_longitude), "Longitude from GPS", &status);
    fits_write_key(fptr, TDOUBLE, "GPS_ALT", &(meta->gps_altitude), "Altitude from GPS", &status);
    fits_write_key(fptr, TINT, "GPS_SATS", &(meta->gps_satellites), "GPS satellites visible", &status);
    fits_write_key(fptr, TDOUBLE, "GPS_TIME", &(meta->gps_fix_time), "GPS fix unix timestamp of update", &status);
    fits_write_key(fptr, TDOUBLE, "GPS_ONLINE", &(meta->gps_online), "NZ if GPS is online, 0 if not", &status);
    fits_write_key(fptr, TUINT, "GPS_FIX", &(meta->gps_fix_mode), "GPS fix mode NOTSEEN,NOFIX,2D,3D", &status);
    fits_write_key(fptr, TUINT, "GPS_STAT", &(meta->gps_status), "GPS status NOFIX,FIX,DGPS", &status);

    // TODO add more keys...
    const char author[] = "jerryjiahaha@gmail.com";
    fits_write_key(fptr, TSTRING, "AUTHOR", const_cast<char *>(author), "Author of the software auto_expo", &status);

    // fits_write_img(fptr, FitsTypeConv.at(meta->imgType), fpixel, nelements, buf, &status);
    fits_write_img(fptr, FitsTypeConv.at(imgType), fpixel, nelements, buf, &status);
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
        abort();
    }
    logger->debug("{} saved", meta->expo_cnt);
}

void ASICamHandler::data_saving_start() {
    if (th_data != nullptr) {
        throw "Data thread should be stopped first";
    }
    auto data_lambda = [this]() {
        logger->info("data saving thread started");
        unsigned long imgSize;
        uint8_t *imgBuf;
        std::shared_ptr<ImgMeta> meta;
        while (!th_expo_stop) {
            mtx_data.lock();
            if (data_q.empty()) {
                mtx_data.unlock();
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                //std::this_thread::yield();
                continue;
            }
            std::tie(imgBuf, imgSize, meta) = data_q.front();
            assert(buf_to_proc.erase(imgBuf));
            data_q.pop();
            mtx_data.unlock();
            logger->debug("pick img data {}-{:p}", meta->expo_cnt, imgBuf);
            // TODO should I copy data first ?
            // TODO dispatch to thread pool
            save_data(imgBuf, imgSize, meta);
        }
        logger->info("saving cached data...");
        // TODO optimize construction, wrap into same method as loop above
        while (!data_q.empty()) {
            std::tie(imgBuf, imgSize, meta) = data_q.front();
            logger->debug("pick img data {:p}", imgBuf);
            assert(buf_to_proc.erase(imgBuf));
            data_q.pop();
            save_data(imgBuf, imgSize, meta);
        }
        logger->info("data saving thread stopped");
    };
    th_data = std::unique_ptr<std::thread>(new std::thread(data_lambda));
}

void ASICamHandler::monitor_loop() {
    if (th_mon != nullptr) {
        throw "Expo thread should be stopped first";
    }
    assert(th_mon == nullptr);
    auto mon_lambda = [this]() {
        logger->info("monitor thread started");
        long temp = 0;
        ASI_BOOL bt = ASI_FALSE;
        // It seems that ASI cam must set cooler first and wait for some while to get valid temp
        std::this_thread::sleep_for(std::chrono::seconds(1));
        while (!th_expo_stop) {
            auto ret = ASIGetControlValue(camId(), ASI_TEMPERATURE, &temp, &bt);
            if (ret) {
                logger->error("[cam-{}] get temp failed: {}", camId(), ret);
            }
            else {
                logger->debug("[cam-{}] get temperature {}", camId(), temp);
                real_temp = temp / 10.0;
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        logger->info("monitor thread ended");
    };
    th_mon = std::unique_ptr<std::thread>(new std::thread(mon_lambda));
}

void ASICamHandler::expo_wait_cooling() {
    // It seems that ASI cam must set cooler first and wait for some while to get valid temp
    std::this_thread::sleep_for(std::chrono::seconds(2));
    // Maybe something is wrong in cooler (or we forgot wo enable cooler) so we have to expose.
    int wait_max = (FLAGS_wait_cooling ? 1000 : 0);
    if (!FLAGS_cooler) {
        logger->warn("no cooler, so no wait for cooling");
        wait_max = 0;
    }
    while (!th_expo_stop && wait_max--) {
        logger->info("wait cooling... {} -> {}", real_temp, cool_temp);
        if (real_temp < cool_temp + 1) {
            break;
        }
        // XXX maybe the temperature check process should be put under statistics thread, and use conditional variable to wakeup this thread
        int tik = 5;
        while (tik-- && !th_expo_stop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void ASICamHandler::expo_loop_start(unsigned long loop_cnt) {
    // this function could only be called when loop is not started
    if (th_expo != nullptr) {
        throw "Expo thread should be stopped first";
    }
    assert(th_expo == nullptr);

    this->loop_cnt = loop_cnt;

    logger->info("imgtype: {} {} {}", imgType, _ImgType.at(imgType), imgType == ASI_IMG_RAW16);
    logger->info("creating circular buffer, height x width: {}x{}, total size: {}", curHeight, curWidth, (unsigned long)curHeight * curWidth * (1 + (imgType == ASI_IMG_RAW16)));
    cir_buf = std::unique_ptr<CircularBuffer>(new CircularBuffer((unsigned long)curHeight * curWidth * (1 + (imgType == ASI_IMG_RAW16)), 20));

    auto expo_lambda = [this]() {
        logger->info("expo thread started");

        unsigned long &expo_cnt = this->expo_cnt;
        unsigned long &loop_cnt = this->loop_cnt;

        expo_wait_cooling();

        while (!th_expo_stop) {
            if (loop_cnt > 0 && expo_cnt >= loop_cnt && !FLAGS_keep_running) {
                break;
            }

            if (loop_cnt > 0 && expo_cnt >= loop_cnt && FLAGS_keep_running) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            int camId = info.CameraID; 
            logger->warn("[cam-{}] start exposure {}", camId, expo_cnt);

            // Construct metadata
            auto meta = std::shared_ptr<ImgMeta>(new ImgMeta);
            *meta = {}; // will be filled

            // TODO add value to measure StartExposure call
            ASIStartExposure(camId, ASI_FALSE);
            auto tim_expo_start = std::chrono::system_clock::now();
            auto tik_expo_start = std::chrono::steady_clock::now();
            addGPSinfo(meta, GPSInfo::gpsInfo);
            logger->debug("[cam-{}] after start exposure", camId);
            ASI_EXPOSURE_STATUS expo_stat = ASI_EXPOSURE_STATUS::ASI_EXP_WORKING;

#if 1
            auto elapse_cnt = (unsigned long)(expo_ms / 10.0);
            while (!th_expo_stop && elapse_cnt) {
                if (expo_ms < 10) {
                    std::this_thread::sleep_for(std::chrono::milliseconds((long)expo_ms));
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                elapse_cnt--;
            }
#endif
            while (expo_stat == ASI_EXPOSURE_STATUS::ASI_EXP_WORKING) {
                auto res = ASIGetExpStatus(camId, &expo_stat);
                traceError(res);
                if (res) {
                    logger->error("[cam-{}] get exp status failed: {}", camId, res);
                    break;
                }
//                logger->trace("[cam-{}] expo status: {}", camId, expo_stat);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
//                std::this_thread::yield();
            }
            auto tik_expo_stop = std::chrono::steady_clock::now() - tik_expo_start;
            if (expo_stat == ASI_EXPOSURE_STATUS::ASI_EXP_SUCCESS) {
                logger->info("[cam-{}] expo success {}", info.CameraID, expo_cnt);
            }
            else {
                logger->error("[cam-{}] expo failed {} {}", info.CameraID, expo_cnt, expo_stat);
            }
    
            unsigned long imgSize;
            uint8_t *imgBuf;
            mtx_data.lock();
            std::tie(imgBuf, imgSize) = cir_buf->gen_buff();
//            assert(buf_to_proc.count(imgBuf) == 0);
            if (buf_to_proc.count(imgBuf)) {
                logger->critical("CircularBuffer is not big enough");
                th_expo_stop = 1;
                mtx_data.unlock();
                continue;
//                throw "CircularBuffer is not big enough";
            }
            buf_to_proc.insert(imgBuf);
            mtx_data.unlock();
            auto res = ASIGetDataAfterExp(camId, imgBuf, imgSize);
            auto tik_acq_end = std::chrono::steady_clock::now() - tik_expo_start;
            logger->info("[cam-{}] data acquired {}", this->camId(), expo_cnt);
            traceError(res);
            if (res) {
                logger->error("[cam-{}] get data failed: {}", camId, res);
                mtx_data.lock();
                buf_to_proc.erase(imgBuf);
                mtx_data.unlock();
                expo_cnt++;
                continue;
            }

            // Adding Data
            meta->expo_cnt = expo_cnt;
            meta->tim_expo_start = std::move(tim_expo_start);
            meta->tik_expo_stop = std::move(tik_expo_stop);
            meta->tik_acq_end = std::move(tik_acq_end);
//            meta->imgHeight = curHeight;
//            meta->imgWidth = curWidth;
//            meta->imgType = imgType;
            meta->real_temp = real_temp;

            mtx_data.lock();
            data_q.push(std::make_tuple(imgBuf, imgSize, meta));
            mtx_data.unlock();
            expo_cnt++;
        }
        logger->info("expo thread stopped");
    };
    th_expo = std::unique_ptr<std::thread>(new std::thread(expo_lambda));
}

using ASICamManager = std::map<int, std::unique_ptr<ASICamHandler>>;
// using ASICamManager = std::map<int, ASICamHandler>;

static ASICamManager camManager;

#define RPC_PORT 9116
static us_listen_socket_t *httpstub = nullptr;
static std::shared_ptr<std::thread> th_http = nullptr;

static void sig_handler(int signum) {
    if (signum == SIGINT) {
        for (auto& cam: camManager) {
            cam.second->stop();
        }
        for (auto& cam: camManager) {
            cam.second->wait_all_threads();
        }
        // close cameras
        for (auto& cam: camManager) {
            ASICloseCamera(cam.second->camId());
        }
        if (GPSInfo::gpsInfo) {
            GPSInfo::gpsInfo->stop();
        }
        if (th_http) {
            if (httpstub) {
                std::cout << "close socket" << std::endl;
                us_listen_socket_close(0, httpstub);
            }
            th_http->join();
        }
        exit(0);
    }
}

int main(int argc, char **argv) {
    gflags::SetUsageMessage("just run the program to auto expo, --help to show command line param");
    gflags::ParseCommandLineFlags(&argc, &argv, false);
    // create color multi threaded logger
    auto console = spdlog::stdout_color_mt(__FILE__);
    console->info("Started!");
    spdlog::set_pattern("%^[%L] [%T.%e] [%n] [tid-%t]%$ %v");
    spdlog::set_level((spdlog::level::level_enum)FLAGS_log_level);
    // console->set_pattern("%^[%L] [%T.%e] [%n] [tid-%t]%$ %v");

    // process singleton lock
    std::ostringstream lockname;
    lockname << "/tmp/" << fs::path(argv[0]).stem().c_str() << ".pid";
    console->info("Create lock {}", lockname.str());
    int lockfd = open(lockname.str().c_str(), O_CREAT|O_CLOEXEC|O_SYNC|O_TRUNC|O_WRONLY, S_IROTH|S_IRGRP|S_IRUSR|S_IWUSR|S_IWGRP|S_IWOTH);
    int lockstate = flock(lockfd, LOCK_EX|LOCK_NB);
    if (lockstate) {
        if (errno == EWOULDBLOCK) {
            console->error("process has been started, do not start new one");
            return -1;
        }
        console->error("other error about flock {}", strerror(errno));
    }
    std::ostringstream oss;
    oss << getpid();
    write(lockfd, oss.str().c_str(), oss.str().size());

    // check output config
    console->info("output dir is {}", FLAGS_output);
    console->info("filename prefix is {}", FLAGS_prefix);

    // init gps info receiver
    GPSInfo::gpsInfo = std::make_shared<GPSInfo>();

    // find cameras
    int numDevices = ASIGetNumOfConnectedCameras();
    console->info("numDevices: {}", numDevices);

    if (numDevices <= 0) {
        console->error("no device found!");
        return -1;
    }

    for (int i = 0; i < numDevices; i++) {
        auto handler = std::unique_ptr<ASICamHandler>(new ASICamHandler);
//        auto *handler = new ASICamHandler;
        // auto handler = ASICamHandler();
        ASIGetCameraProperty(handler->pInfo(), i);
        // ASIGetCameraProperty(handler.pInfo(), i);
        handler->setInfo();
        // handler.setInfo();
        // camManager.emplace(handler.camId(), std::move(handler));
//        camManager[handler->camId()] = std::make_unique<ASICamHandler>(handler);
        camManager[handler->camId()] = std::move(handler);
    }

    // register signal
    std::signal(SIGINT, sig_handler);

    // init cameras
    for (auto& cam: camManager) {
        int camId = cam.second->camId();
        // TODO wrap these ugly steps

        // open
        console->info("open cam {}: {}", camId, cam.second->camName());
        int res = ASIOpenCamera(camId);
        if (res) {
            console->error("open failed: {}", res);
            return -2;
        }
        res = ASIInitCamera(camId);
        if (res) {
            console->error("init failed: {}", res);
            return -3;
        }

        ASI_ID asicamid = {};
        res = ASIGetID(camId, &asicamid);
        if (res) {
            console->error("{} getid failed: {}", camId, res);
        }
        else {
            console->info("Cam flash ID: {}", asicamid.id);
            cam.second->prodId = (char *)(asicamid.id);
        }
#ifdef SETCAMID
        const std::string names[2] = {"R", "L"};
        int static cnt = 0;
        std::string name = names[cnt%2] + std::to_string(cnt);
        ASI_ID newid;
        memcpy(newid.id, name.c_str(), sizeof(newid.id)-1);
        ASISetID(camId, newid);
        ++cnt;
#endif
        print_asicam_info(cam.second->pInfo());

        // init control values
        int iNumOfCtrl = 0;
        res = ASIGetNumOfControls(camId, &iNumOfCtrl);
        if (res) {
            console->error("get num of control failed: {}", res);
            return -4;
        }
        console->info("Num of control: {}", iNumOfCtrl);
        for (int i = 0; i < iNumOfCtrl; i++) {
            ASI_CONTROL_CAPS ctrlCaps = {};
            ASIGetControlCaps(camId, i, &ctrlCaps);
            print_asicam_control_caps(&ctrlCaps);
            cam.second->setCtrlCaps(ctrlCaps);
            long plValue = 0;
            ASI_BOOL pbAuto = ASI_FALSE;
            ASIGetControlValue(camId, ctrlCaps.ControlType, &plValue, &pbAuto);
            console->info("current {} for {} is val: {}, auto: {}", ctrlCaps.Name, camId, plValue, pbAuto);
            cam.second->saveCtrlVal(ctrlCaps.ControlType, plValue, pbAuto);
        }
    }

    // init cooler
    for (auto& cam: camManager) {
        console->info("cmd set cool target temp {}", FLAGS_cool_temp);
        cam.second->setCoolerTemp(FLAGS_cool_temp);
        console->info("cmd set cooler {}", FLAGS_cooler);
        cam.second->setCtrlVal(ASI_COOLER_ON, FLAGS_cooler, ASI_FALSE);
    }

    // setup cameras
    for (auto& cam: camManager) {
        console->info("cmd set gain {}", FLAGS_gain);
        cam.second->setCtrlVal(ASI_GAIN, FLAGS_gain, ASI_FALSE);
        cam.second->setCtrlVal(ASI_GAMMA, 50, ASI_FALSE);
        console->info("cmd set mono bin {}", FLAGS_mono_bin);
        cam.second->setCtrlVal(ASI_MONO_BIN, FLAGS_mono_bin, ASI_FALSE);
        cam.second->setCtrlVal(ASI_HIGH_SPEED_MODE, 0, ASI_FALSE);
        cam.second->setCtrlVal(ASI_BANDWIDTHOVERLOAD, 75, ASI_FALSE);
        cam.second->setCtrlVal(ASI_FLIP, 0, ASI_FALSE);

        console->info("cmd set expo {} ms", FLAGS_expo_ms);
        assert(cam.second->setExpoMilliSec(FLAGS_expo_ms) == ASI_SUCCESS);

       // sleep(1);
       // long temp = 0;
       // ASI_BOOL bt;
       // assert(ASIGetControlValue(camId, ASI_TEMPERATURE, &temp, &bt) == ASI_SUCCESS);
       // console->info("temperature {}", temp);

    }

    // ROI and BIN
    for (auto& cam: camManager) {
        console->info("cmd set bin {}", FLAGS_bin);
        console->info("cmd set img type {}", FLAGS_img_type);
        ASI_IMG_TYPE img_type = ASI_IMG_RAW16;
        if (FLAGS_img_type == "RAW8") {
            img_type = ASI_IMG_RAW8;
        }
        cam.second->setROIFormat(FLAGS_bin, img_type);
    }

    /*
     * Threads:
     *      - rpc thread
     *      - expo threads
     *      - statistics threads
     *      - data saving threads
     */

    // XXX ugly thread to check online number of camera
    std::thread([console]() {
        // check number of online camera
        int conn_err_count = 0;
        while (1) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            console->info("check online camera num {}", ASIGetNumOfConnectedCameras());
            // XXX another method is write udev rules to handle hotplug event (when usb connected, restart this program)
            if (ASIGetNumOfConnectedCameras() != camManager.size()) {
                conn_err_count++;
                if (conn_err_count > 3) {
                    console->critical("maybe new camera attached or usb is buggy, consider restart");
                    for (auto& cam: camManager) {
                    cam.second->stop();
                    }
                    alarm(20);
                }
            }
        }
    }).detach();
    
    // init http microservice/ipc ... simple and ugly...
    th_http = std::make_shared<std::thread>([console]() {
        auto app = uWS::App().get("/ping", [](auto *res, auto *) {
            res->end("pong");
        }).get("/start_expo/:expo_cnt", [=](auto *res, auto *req) {
            auto str = req->getParameter(0);
            unsigned long expo_cnt = 0;

            std::from_chars(std::cbegin(str), std::cend(str), expo_cnt);
            //std::stoi(req->getParameter(0));
            console->info("set new target expo count {}", expo_cnt);
            std::for_each(camManager.begin(), camManager.end(), [expo_cnt](auto &cam) {

                cam.second->reset_expo_count(expo_cnt);
            });
            res->end("ok");
        }).get("/stop_expo", [console](auto *res, auto *req) {
            console->info("stop expo for some while");
            std::for_each(camManager.begin(), camManager.end(), [](auto &cam) {
                cam.second->stop_expo_count();
           });
            res->end("ok");
        }).listen(RPC_PORT, [console](auto *token) {
            if (token) {
                httpstub = token;
                console->info("Server listening {}", RPC_PORT);
            }
        }).run();
        console->warn("Server exit");
    });

    // exposure
    //   snap mode
    for (auto& cam: camManager) {
#if 0
        int camId = cam.second->camId();
        console->info("cam {} start exposure", camId);
        int res = ASIStartExposure(camId, ASI_FALSE);
        auto start = std::chrono::steady_clock::now();
        if (res) {
            console->error("start expo {} failed: {}", camId, res);
        }
        ASI_EXPOSURE_STATUS status;
        while (1) {
            ASIGetExpStatus(camId, &status);
            if (status != ASI_EXP_WORKING) break;
        }
        auto stop = std::chrono::steady_clock::now();
        auto delta = stop - start;
        std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(delta).count() << std::endl;
        std::cout << status << std::endl;
        assert(status == ASI_EXP_SUCCESS);
#else
        cam.second->monitor_loop();
        cam.second->data_saving_start();
        console->info("will expo {} times", FLAGS_expo_count);
        cam.second->expo_loop_start(FLAGS_expo_count);
#endif
    }

    for (auto& cam: camManager) {
        cam.second->wait_all_threads();
    }

    // close cameras
    for (auto& cam: camManager) {
        ASICloseCamera(cam.second->camId());
    }

    if (th_http) {
        if (httpstub) {
            us_listen_socket_close(0, httpstub);
        }
        th_http->join();
    }

    return 0;
}

// vim: sw=4 ts=4 et :
