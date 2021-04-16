#include "MarianInterface.h"
#include "service.h"
#include "parser.h"
#include "response.h"

namespace  {
marian::Ptr<marian::Options> MakeOptions(std::string path_to_yaml) {
    std::cerr << "In options" << std::endl;
    std::string model_path = path_to_yaml;
    std::vector<std::string> args = {"marian-decoder", "-c", model_path, "--cpu-threads", "8", "--mini-batch-words", "1000"};

    std::vector<char *> argv;
    argv.reserve(args.size());

    for (int i = 0; i < args.size(); ++i) {
        argv.push_back(const_cast<char *>(args[i].c_str()));
    }
    std::cerr << "CP obj" << std::endl;
    auto cp = marian::bergamot::createConfigParser();
    std::cerr << "parse" << std::endl;
    auto options = cp.parseOptions(argv.size(), &argv[0], true);
    std::cerr << "Parsed options" << std::endl;
    return options;
}
} // Anonymous namespace

MarianInterface::MarianInterface(std::string path_to_model_dir) : service_(MakeOptions(path_to_model_dir)) {}

std::string MarianInterface::translate(std::string input) {
    using marian::bergamot::Response;

    // Wait on future until Response is complete
    std::future<marian::bergamot::Response> responseFuture = service_.translate(std::move(input));
    responseFuture.wait();
    marian::bergamot::Response response = responseFuture.get();

    return response.target.text;
}

