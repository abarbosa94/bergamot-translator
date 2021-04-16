#include <cstdlib>
#include <future>
#include <iostream>
#include <sstream>

#include "common/definitions.h"
#include "common/utils.h"
#include "marian.h"
#include "translator/parser.h"
#include "translator/response.h"
#include "translator/service.h"
#include "translator/MarianInterface.h"

int main(int argc, char *argv[]) {
  MarianInterface * mymodel = new MarianInterface("/mnt/Storage/students/esen/enes.student.tiny11/config.intgemm8bitalpha.yml");
  std::cerr << mymodel->translate("Test") << std::endl;
  delete mymodel;
  mymodel = new MarianInterface("/mnt/Storage/students/esen/enes.student.tiny11/config.intgemm8bitalpha.yml");
  std::cerr << mymodel->translate("Test two") << std::endl;
  delete mymodel;
  

  return 0;
}
