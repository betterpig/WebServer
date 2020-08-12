#include <string>
#include <time.h>

using namespace std;

struct Session
{
    const int delay=10*60;
    const string domain="";
    const string path="";
    string session_id;
    time_t expire;
    bool is_authorized;
    bool is_reset;
    Session():is_authorized(false),is_reset(true)
    {
        expire=time(nullptr)+delay;
        session_id=to_string(time(nullptr));
        srand((unsigned)time(NULL));
        for(int i = 0; i < 10;i++ )
        {
            session_id+=to_string(rand()%9);
        }
    }
};