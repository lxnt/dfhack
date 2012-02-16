#include "Internal.h"
#include "df/api.h"
#include "Core.h"
#include "MiscUtils.h"
#include "VersionInfo.h"
#include "MemAccess.h"
#include "tinythread.h"


static tthread::mutex *known_mutex = NULL;

class dfhack_essentials : public df::api::essentials {
    
private:
    std::map<std::string, void *> v_table;

public:

    std::string readClassName(void *vtable) { return DFHack::Core::getInstance().p->doReadClassName(vtable); }
    void lock() { known_mutex->lock(); }
    void unlock() { known_mutex->unlock(); }
    std::map<std::string, void *>& getVtable() { return v_table; }
    void * getGlobal(const char *name) {
	void *tmp_;

	if ( DFHack::Core::getInstance().vinfo->getAddress(name, tmp_) );
	    return tmp_; 
	return NULL;
    }

    void Init() {
	if (!known_mutex)
	    known_mutex = new tthread::mutex();

    }
};

static dfhack_essentials desse;

void DaStaInit(void) {
    // this is to be called when offsetgroups are ready.
    desse.Init();
    df::global::InitGlobals(&desse);
}

