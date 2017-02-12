#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include "httpd.h"

using namespace std;

void usage(char * argv0)
{
	cerr << "Usage: " << argv0 << " listen_port docroot_dir" << endl;
}

int main(int argc, char *argv[])
{
	if (argc < 4) {
		usage(argv[0]);
		return 1;
	}




	long int port = strtol(argv[1], NULL, 10);

 

	if (errno == EINVAL || errno == ERANGE) {
		usage(argv[0]);
		return 2;
	}

	if (port <= 0 || port > USHRT_MAX) {
		cerr << "Invalid port: " << port << endl;
		return 3;
	}

	string doc_root = argv[2];

 

  printf("argc is %d\n", argc);
  cout << argv[3] << endl;
  if ( strcmp(argv[3], "pool")==0 && argc == 5){
    int pool_size = strtol(argv[4], NULL, 10);
    start_httpd(port, doc_root, 1, pool_size);
    return 0;
  } else if (argv[3]=="pool"&& argc !=5) {
    usage(argv[0]);
    return 1;
  }


  printf("no pool one\n");

	start_httpd(port, doc_root, 0, 0);

	return 0;
}
