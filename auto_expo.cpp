// CPP STANDARD: c++17
#include <iostream>
#include <map>
#include <ctime>
#include <utility>
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <cstring>
#include <memory>
#include <sstream>
#include <queue>
#include <atomic>
#include <mutex>
#include <set>
#include <tuple>
#include <chrono>
#include <thread>

#include <filesystem>

#include <unistd.h>

#include <fitsio.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <gflags/gflags.h>

#include "ASICamera2.h"

namespace fs = std::filesystem;

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
 *   - find cameras
 *   - init cooler
 *   - setup 
 *   - start expo
 */

// ASI_ERR_CHECK

const std::string output_format[] = {
    "hex",
    "bin",
    "fits",
};

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
    std::cout << "IsUSB3Host: " << info->IsUSB3Host << ",\t";
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
typedef struct {
    std::chrono::system_clock::time_point tim_expo_start;
    std::chrono::steady_clock::duration tik_expo_stop;
    std::chrono::steady_clock::duration tik_acq_end;
    int imgHeight;
    int imgWidth;
    ASI_IMG_TYPE imgType;
} ImgMeta;

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
                std::this_thread::sleep_for(std::chrono::seconds(10));
                abort();
            });
            tid.detach();
            th_expo_stop = 1;
            if (th_expo) {
                th_expo->join();
            }
        }
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
        auto setExpoMilliSec(float _expo_ms) {
            if (_expo_ms < 0 || _expo_ms > 60*1000) {
                logger->error("invalid expo_ms setup {}", _expo_ms);
                return ASI_ERROR_GENERAL_ERROR;
            }
            expo_ms = _expo_ms;
            long expo_us = (long)(expo_ms * 1000);
            return setCtrlVal(ASI_EXPOSURE, expo_us, ASI_FALSE);
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
        int curHeight = 0;
        int curWidth = 0;
        int curBin = 0;
        ASI_IMG_TYPE imgType = ASI_IMG_END;

        void expo_loop_start(unsigned long loop_cnt = 0);
        void data_saving_start();
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
    private:
        ASI_CAMERA_INFO info = {};
        std::map<ASI_CONTROL_TYPE, ASI_CONTROL_CAPS> ctrlMap;
        std::map<ASI_CONTROL_TYPE, long> ctrlVal;
        std::map<ASI_CONTROL_TYPE, ASI_BOOL> ctrlValAuto;

        static int _log_cnt;
        std::shared_ptr<spdlog::logger> logger;
        float expo_ms = 100;

        std::unique_ptr<CircularBuffer> cir_buf = nullptr;
        std::set<void *> buf_to_proc;
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
    static unsigned long cnt;
    std::ostringstream fname;
    fname << "!" << "output-" << cnt++ << ".fits" << ".gz";
    fitsfile *fptr;
    int status = 0;
    long fpixel = 1, naxis = 2, nelements = 0;
    long naxes[2] = {meta->imgWidth, meta->imgHeight};
    nelements = naxes[0] * naxes[1];
    fits_create_file(&fptr, fname.str().c_str(), &status);
    fits_create_img(fptr, ASIImg2Fits.at(meta->imgType), naxis, naxes, &status);
    fits_write_date(fptr, &status);
    char input[] = "test";
    fits_write_key(fptr, TSTRING, "TEST", input, "test comment", &status);
    fits_write_img(fptr, FitsTypeConv.at(meta->imgType), fpixel, nelements, buf, &status);
    fits_close_file(fptr, &status);
    if (status) {
        fits_report_error(stderr, status);
    }
}

void ASICamHandler::data_saving_start() {
    if (th_data != nullptr) {
        throw "Data thread should be stopped first";
    }
    logger->info("data saving thread started");
    auto data_lambda = [this]() {
        unsigned long imgSize;
        uint8_t *imgBuf;
        std::shared_ptr<ImgMeta> meta;
        while (!th_expo_stop) {
            mtx_data.lock();
            if (data_q.empty()) {
                mtx_data.unlock();
                std::this_thread::yield();
                continue;
            }
            std::tie(imgBuf, imgSize, meta) = data_q.front();
            assert(buf_to_proc.erase(imgBuf));
            data_q.pop();
            mtx_data.unlock();
            logger->info("pick img data {:p}", imgBuf);
            // TODO should I copy data first ?
            // TODO dispatch to thread pool
            save_data(imgBuf, imgSize, meta);
        }
        logger->info("saving cached data...");
        // TODO optimize construction, wrap into same method as loop above
        while (!data_q.empty()) {
            std::tie(imgBuf, imgSize, meta) = data_q.front();
            logger->info("pick img data {:p}", imgBuf);
            assert(buf_to_proc.erase(imgBuf));
            data_q.pop();
            save_data(imgBuf, imgSize, meta);
        }
        logger->info("data saving thread stopped");
    };
    th_data = std::unique_ptr<std::thread>(new std::thread(data_lambda));
}

void ASICamHandler::expo_loop_start(unsigned long loop_cnt) {
    // this function could only be called when loop is not started
    if (th_expo != nullptr) {
        throw "Expo thread should be stopped first";
    }
    assert(th_expo == nullptr);

    logger->info("imgtype: {} {} {}", imgType, _ImgType.at(imgType), imgType == ASI_IMG_RAW16);
    logger->info("creating circular buffer, height x width: {}x{}, total size: {}", curHeight, curWidth, (unsigned long)curHeight * curWidth * (1 + (imgType == ASI_IMG_RAW16)));
    cir_buf = std::unique_ptr<CircularBuffer>(new CircularBuffer((unsigned long)curHeight * curWidth * (1 + (imgType == ASI_IMG_RAW16)), 20));

    auto expo_lambda = [this, loop_cnt]() {
        logger->info("expo thread started");
        unsigned long expo_cnt = 0;
        while (!th_expo_stop) {
            if (loop_cnt > 0 && expo_cnt >= loop_cnt) {
                break;
            }

            int camId = info.CameraID; 
            logger->info("[cam-{}] start exposure {}", camId, expo_cnt);
            auto tim_expo_start = std::chrono::system_clock::now();
            auto tik_expo_start = std::chrono::steady_clock::now();
            ASIStartExposure(camId, ASI_FALSE);
            logger->info("[cam-{}] after exposure", camId);
            ASI_EXPOSURE_STATUS expo_stat = ASI_EXPOSURE_STATUS::ASI_EXP_WORKING;

            auto elapse_cnt = (unsigned long)(expo_ms / 10.0);
            while (!th_expo_stop && elapse_cnt) {
                if (expo_ms < 10) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                elapse_cnt--;
            }
            while (expo_stat == ASI_EXPOSURE_STATUS::ASI_EXP_WORKING) {
                auto res = ASIGetExpStatus(camId, &expo_stat);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                if (res) {
                    logger->error("[cam-{}] get exp status failed: {}", camId, res);
                    break;
                }
//                logger->trace("[cam-{}] expo status: {}", camId, expo_stat);
            }
            auto tik_expo_stop = std::chrono::steady_clock::now() - tik_expo_start;
            if (expo_stat == ASI_EXPOSURE_STATUS::ASI_EXP_SUCCESS) {
                logger->info("[cam-{}] expo success {}", info.CameraID, expo_cnt);
            }
    
            unsigned long imgSize;
            uint8_t *imgBuf;
            mtx_data.lock();
            std::tie(imgBuf, imgSize) = cir_buf->gen_buff();
            assert(buf_to_proc.count(imgBuf) == 0);
            if (buf_to_proc.count(imgBuf)) {
                throw "CircularBuffer is not big enough";
            }
            buf_to_proc.insert(imgBuf);
            mtx_data.unlock();
            auto res = ASIGetDataAfterExp(camId, imgBuf, imgSize);
            auto tik_acq_end = std::chrono::steady_clock::now() - tik_expo_start;
            if (res) {
                logger->error("[cam-{}] get data failed: {}", camId, res);
                mtx_data.lock();
                buf_to_proc.erase(imgBuf);
                mtx_data.unlock();
                expo_cnt++;
                continue;
            }

            // Adding Data
            // Construct metadata
            auto meta = std::shared_ptr<ImgMeta>(new ImgMeta);
            *meta = {};
            meta->tim_expo_start = std::move(tim_expo_start);
            meta->tik_expo_stop = std::move(tik_expo_stop);
            meta->tik_acq_end = std::move(tik_acq_end);
            meta->imgHeight = curHeight;
            meta->imgWidth = curWidth;
            meta->imgType = imgType;

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

int main(int argc, char *argv[]) {
    // create color multi threaded logger
    auto console = spdlog::stdout_color_mt("console");
    console->info("Started!");
    spdlog::set_pattern("%^[%L] [%T.%e] [%n] [tid-%t]%$ %v");
//    spdlog::set_level(spdlog::level::trace);
    // console->set_pattern("%^[%L] [%T.%e] [%n] [tid-%t]%$ %v");
    
    // find cameras
	int numDevices = ASIGetNumOfConnectedCameras();
    console->info("numDevices: {}", numDevices);

//    std::map<int, ASI_CAMERA_INFO> camInfoHolder;
    ASICamManager camManager;

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

#if 0
        ASI_ID asicamid = {};
        res = ASIGetID(camId, &asicamid);
        if (res) {
            console->error("getid failed: {}", res);
        }
        console->info("Cam flash ID: {}", asicamid.id);
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

    // setup cameras
    for (auto& cam: camManager) {
        cam.second->setCtrlVal(ASI_GAIN, 100, ASI_FALSE);
        cam.second->setCtrlVal(ASI_COOLER_ON, 0, ASI_FALSE);
        cam.second->setCtrlVal(ASI_GAMMA, 50, ASI_FALSE);
        cam.second->setCtrlVal(ASI_MONO_BIN, 1, ASI_FALSE);
        cam.second->setCtrlVal(ASI_HIGH_SPEED_MODE, 0, ASI_FALSE);
        cam.second->setCtrlVal(ASI_BANDWIDTHOVERLOAD, 75, ASI_FALSE);
        cam.second->setCtrlVal(ASI_FLIP, 0, ASI_FALSE);

        cam.second->setExpoMilliSec(200);

       // sleep(1);
       // long temp = 0;
       // ASI_BOOL bt;
       // assert(ASIGetControlValue(camId, ASI_TEMPERATURE, &temp, &bt) == ASI_SUCCESS);
       // console->info("temperature {}", temp);

    }

    // ROI and BIN
    for (auto& cam: camManager) {
        int bin = 2;
        ASI_IMG_TYPE img_type = ASI_IMG_RAW16;
        cam.second->setROIFormat(bin, img_type);
    }

    /*
     * Threads:
     *      - expo threads
     *      - statistics threads
     *      - data saving threads
     */

    // exposure
    //   snap mode
    for (auto& cam: camManager) {
        int camId = cam.second->camId();
        console->info("cam {} start exposure", camId);
        int res = ASIStartExposure(camId, ASI_FALSE);
        if (res) {
            console->error("start expo {} failed: {}", camId, res);
        }
        cam.second->data_saving_start();
        cam.second->expo_loop_start(5);
    }

    for (auto& cam: camManager) {
        cam.second->expo_loop_wait();
        cam.second->data_saving_wait();
    }

    // close cameras
    for (auto& cam: camManager) {
        ASICloseCamera(cam.second->camId());
    }

    return 0;
}

// vim: sw=4 ts=4 et :
