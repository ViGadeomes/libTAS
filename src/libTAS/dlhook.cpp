/*
    Copyright 2015-2016 Clément Gallet <clement.gallet@ens-lyon.org>

    This file is part of libTAS.

    libTAS is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    libTAS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with libTAS.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dlhook.h"
#include "logging.h"
#include <cstring>

namespace libtas {

/* Set of libraries that are loaded by the game,
 * either at startup (link time) or using the dl functions.
 */
static std::vector<std::string> libraries;

void add_lib(std::string library)
{
    libraries.push_back(library);
    debuglog(LCF_HOOK, "Lib vect content:");
    for (auto const& itr : libraries)
        debuglog(LCF_HOOK, itr);
}

std::string find_lib(const char* library)
{
    for (auto const& itr : libraries)
        if (itr.find(library) != std::string::npos)
            return itr;

    std::string emptystring;
    return emptystring;
}

static void *my_dlopen(const char *file, int mode, void *dl_caller) {
    void *result;
    debuglog(LCF_HOOK, __func__, " call with file ", file);
    dlenter();
    result = dlopen(file, mode);
    dlleave();
    debuglog(LCF_HOOK, "Lib vect content:");
    for (auto const& itr : libraries)
        debuglog(LCF_HOOK, itr);

    if (result && (file != nullptr))
        libraries.push_back(std::string(file));
    return result;
}

static int my_dlclose(void *handle) {
    int result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dlclose(handle);
    dlleave();
    return result;
}

static void *my_dlsym(void *handle, const char *name, void *dl_caller) {
    void *result;
    debuglog(LCF_HOOK, __func__, " call with function ", name);
    dlenter();
    /* Try to link to an already defined function first */
    result = dlsym(RTLD_DEFAULT, name);
    if (result == NULL)
        result = dlsym(handle, name);
    dlleave();
    return result;
}

static void *my_dlvsym(void *handle, const char *name, const char *version, void *dl_caller) {
    void *result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dlvsym(handle, name, version);
    dlleave();
    return result;
}

static char *my_dlerror(void) {
    char *result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dlerror();
    dlleave();
    return result;
}

static int my_dladdr(const void *address, Dl_info *info) {
    int result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dladdr(address, info);
    dlleave();
    return result;
}

static int my_dladdr1(const void *address, Dl_info *info, void **extra_info, int flags) {
    int result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dladdr1(address, info, extra_info, flags);
    dlleave();
    return result;
}

static int my_dlinfo(void *handle, int request, void *arg, void *dl_caller) {
    int result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dlinfo(handle, request, arg);
    dlleave();
    return result;
}

static void *my_dlmopen(Lmid_t nsid, const char *file, int mode, void *dl_caller) {
    void *result;
    DEBUGLOGCALL(LCF_HOOK);
    dlenter();
    result = dlmopen(nsid, file, mode);
    dlleave();
    return result;
}

static struct dlfcn_hook *old_dlfcn_hook;

static struct dlfcn_hook my_dlfcn_hook = {
    dlopen   : my_dlopen,
    dlclose  : my_dlclose,
    dlsym    : my_dlsym,
    dlvsym   : my_dlvsym,
    dlerror  : my_dlerror,
    dladdr   : my_dladdr,
    dladdr1  : my_dladdr1,
    dlinfo   : my_dlinfo,
    dlmopen  : my_dlmopen,
    pad      : {0, 0, 0, 0}
};

static int depth;
void dlenter(void) { if (!depth++) _dlfcn_hook = old_dlfcn_hook; }
void dlleave(void) { if (!--depth) _dlfcn_hook = &my_dlfcn_hook; }

void dlhook_init(void)
{
    static int inited = 0;
    if (!inited) {
        old_dlfcn_hook = _dlfcn_hook;
        _dlfcn_hook = &my_dlfcn_hook;
        inited = 1;
    }
}

void dlhook_end(void)
{
    _dlfcn_hook = old_dlfcn_hook;
}

}
