#include <pthread.h>
#include <iostream>
#include <vector>
using namespace std;

class singleton {
   private:
    static singleton* instance;
    singleton();

   public:
    static singleton* getInstance() {
        if (instance == NULL) {
            pthread_mutex_init();
            pthread_mutex_lock(mutex);
            if (instance == NULL) {
                instance = new singleton();
            }
            pthread_mutex_destory(mutex);
        }
    }

    ~singleton();
};
