#include <nan.h>
#include <unistd.h>

#include "src/demo.h"
#include "src/types.h"

using namespace Nan;

template<typename T>
class VideoDetectionWorker : public AsyncProgressWorkerBase<T> {
 public:
  VideoDetectionWorker(Callback *callback, Callback *progress, InputOptions opts)
    : AsyncProgressWorkerBase<T>(callback), progressCb(progress), opts(opts) {}
  ~VideoDetectionWorker() {}

  void Execute(const typename AsyncProgressWorkerBase<T>::ExecutionProgress& progress) {
    start_demo(opts, progress);
  }

  void HandleProgressCallback(const T *data, size_t size) {
    HandleScope scope;
    const int argc = 3;
    v8::Local<v8::Array> results = Nan::New<v8::Array>();
    for(int i = 0; i < data->numberOfResults; i++) {
        RecognitionResult result = data->recognitionResults[i];
        v8::Local<v8::Object> obj = Nan::New<v8::Object>();
        obj->Set(Nan::New("x").ToLocalChecked(), Nan::New<v8::Number>(result.x));
        obj->Set(Nan::New("y").ToLocalChecked(), Nan::New<v8::Number>(result.y));
        obj->Set(Nan::New("w").ToLocalChecked(), Nan::New<v8::Number>(result.w));
        obj->Set(Nan::New("h").ToLocalChecked(), Nan::New<v8::Number>(result.h));
        obj->Set(Nan::New("prob").ToLocalChecked(), Nan::New<v8::Number>(result.prob));
        obj->Set(Nan::New("name").ToLocalChecked(), Nan::New<v8::String>(result.name).ToLocalChecked());
        results->Set(i, obj);
        i++;
    }

    v8::Local<v8::Value> argv[argc] = {
        Nan::NewBuffer(data->modifiedFrame, data->modifiedFrameSize).ToLocalChecked(),
        Nan::NewBuffer(data->originalFrame, data->originalFrameSize).ToLocalChecked(),
        results,
    };

    progressCb->Call(argc, argv);
  }

  private:
    Callback* progressCb;
    InputOptions opts;
};

class ImageDetectionWorker : public AsyncWorker {
 public:
  ImageDetectionWorker(Callback *callback, InputOptions opts)
    : AsyncWorker(callback), opts(opts) {}
  ~ImageDetectionWorker() {}

  // Executed inside the worker-thread.
  // It is not safe to access V8, or V8 data structures
  // here, so everything we need for input and output
  // should go on `this`.
  void Execute () {
    output = start_image_demo(opts);
  }

  // Executed when the async work is complete
  // this function will be run inside the main event loop
  // so it is safe to use V8 again
  void HandleOKCallback () {
    HandleScope scope;
    const int argc = 3;
    v8::Local<v8::Array> results = Nan::New<v8::Array>();
    for(int i = 0; i < output->numberOfResults; i++) {
        RecognitionResult result = output->recognitionResults[i];
        v8::Local<v8::Object> obj = Nan::New<v8::Object>();
        obj->Set(Nan::New("x").ToLocalChecked(), Nan::New<v8::Number>(result.x));
        obj->Set(Nan::New("y").ToLocalChecked(), Nan::New<v8::Number>(result.y));
        obj->Set(Nan::New("w").ToLocalChecked(), Nan::New<v8::Number>(result.w));
        obj->Set(Nan::New("h").ToLocalChecked(), Nan::New<v8::Number>(result.h));
        obj->Set(Nan::New("prob").ToLocalChecked(), Nan::New<v8::Number>(result.prob));
        obj->Set(Nan::New("name").ToLocalChecked(), Nan::New<v8::String>(result.name).ToLocalChecked());
        results->Set(i, obj);
        i++;
    }

    v8::Local<v8::Value> argv[argc] = {
        Nan::NewBuffer(output->modifiedFrame, output->modifiedFrameSize).ToLocalChecked(),
        Nan::NewBuffer(output->originalFrame, output->originalFrameSize).ToLocalChecked(),
        results,
    };

    callback->Call(argc, argv);
  }

 private:
  InputOptions opts;
  WorkerData* output;
};

void detect(const Nan::FunctionCallbackInfo<v8::Value>& arguments) {
  v8::Local<v8::Object> opts = arguments[0].As<v8::Object>();

  v8::Local<v8::Function> function;
  Nan::Callback callback(function);
  Nan::Callback *progress = new Nan::Callback(arguments[1].As<Function>());

  v8::Local<v8::Value> cfgFile = Nan::Get(opts, Nan::New("cfg").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value cfgFileStr(cfgFile);
  char* cfgfile = *cfgFileStr;

  v8::Local<v8::Value> weightFile = Nan::Get(opts, Nan::New("weights").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value weightFileStr(weightFile);
  char* weightfile = *weightFileStr;

  v8::Local<v8::Value> dataFile = Nan::Get(opts, Nan::New("data").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value dataFileStr(dataFile);
  char* datafile = *dataFileStr;

  int cameraIndex = Nan::To<int>(Nan::Get(opts, Nan::New("cameraIndex").ToLocalChecked()).ToLocalChecked()).FromMaybe(0);

  v8::Local<v8::Value> videoFile = Nan::Get(opts, Nan::New("video").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value videoFileStr(videoFile);
  char* videofile = *videoFileStr;

  InputOptions inputOptions;
  strcpy(inputOptions.cfgfile, cfgfile);
  strcpy(inputOptions.weightfile, weightfile);
  strcpy(inputOptions.datafile, datafile);
  if (strcmp("undefined", inputOptions.videofile) != 0) {
    strcpy(inputOptions.videofile, videofile);
  }

  inputOptions.cameraIndex = cameraIndex;

  Nan::AsyncQueueWorker(new VideoDetectionWorker<WorkerData>(&callback, progress, inputOptions));
}

void detectImage(const Nan::FunctionCallbackInfo<v8::Value>& arguments) {
  v8::Local<v8::Object> opts = arguments[0].As<v8::Object>();

  v8::Local<v8::Function> function;
  Nan::Callback *callback = new Callback(arguments[1].As<Function>());

  v8::Local<v8::Value> cfgFile = Nan::Get(opts, Nan::New("cfg").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value cfgFileStr(cfgFile);
  char* cfgfile = *cfgFileStr;

  v8::Local<v8::Value> weightFile = Nan::Get(opts, Nan::New("weights").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value weightFileStr(weightFile);
  char* weightfile = *weightFileStr;

  v8::Local<v8::Value> dataFile = Nan::Get(opts, Nan::New("data").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value dataFileStr(dataFile);
  char* datafile = *dataFileStr;

  v8::Local<v8::Value> imageFile = Nan::Get(opts, Nan::New("image").ToLocalChecked()).ToLocalChecked();
  v8::String::Utf8Value imageFileStr(imageFile);
  char* imagefile = *imageFileStr;

  InputOptions inputOptions;
  strcpy(inputOptions.cfgfile, cfgfile);
  strcpy(inputOptions.weightfile, weightfile);
  strcpy(inputOptions.datafile, datafile);
  strcpy(inputOptions.imagefile, imagefile);

  Nan::AsyncQueueWorker(new ImageDetectionWorker(callback, inputOptions));
}

void Init(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  v8::Local<v8::Object> obj = Nan::New<v8::Object>();
  obj->Set(Nan::New("detect").ToLocalChecked(), Nan::New<v8::FunctionTemplate>(detect)->GetFunction());
  obj->Set(Nan::New("detectImage").ToLocalChecked(), Nan::New<v8::FunctionTemplate>(detectImage)->GetFunction());
  module->Set(Nan::New("exports").ToLocalChecked(), obj);
}

NODE_MODULE(addon, Init)