#include "service.h"


class MarianInterface {
public:
    MarianInterface(std::string path_to_model_dir);
    std::string translate(std::string in);
private:
    marian::bergamot::Service service_;
};