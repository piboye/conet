/*
 * =====================================================================================
 *
 *       Filename:  tls_bench.cpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  2014年10月31日 22时37分41秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  piboye
 *   Organization:  
 *
 * =====================================================================================
 */
#include <stdlib.h>
#include "pb2sqlite.h"

#include "base/src/test.pb.h"

#include "gflags/gflags.h"
#include "glog/logging.h"



int main(int argc, char * argv[])
{
    
  google::ParseCommandLineFlags(&argc, &argv, false); 

  conet::Pb2Sqlite db;

  int ret = 0;

  Test d;

  ret = db.init("./test.db", "test");
  if (ret)
  {
      LOG(ERROR)<<"init db failed!";
      return 0;
  }

  db.create_table(d);
    
  d.set_id(2);
  d.set_name("abc");
  db.insert(d);

  Test w;
  w.set_id(2);
  std::vector<Test*> result;
  db.get_all(w, &result);
  for (size_t i=0; i<result.size(); ++i)
  {
    LOG(INFO)<<result[i]->DebugString();
  }

  return 0;
}
