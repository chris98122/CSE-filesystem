// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <map>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define MAX(a, b) ((a) > (b) ? (a) : (b))
yfs_client::yfs_client()
{
    ec = new extent_client_cache();

    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{

    lc = new lock_client_cache(lock_dst);

    printf("extent_client:%s\n", extent_dst.c_str());
    lc->acquire(1);
    ec = new extent_client_cache(extent_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
    int w;
    ec->flush(1, w);
    lc->release(1);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

bool yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE)
    {
        printf("isfile: %lld is a file\n", inum);
        return true;
    }
    printf("isfile: %lld is a dir\n", inum);

    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool yfs_client::issymlink(inum inum)
{
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_SYMLINK)
    {
        printf("issymlink: %lld is a symlink\n", inum);
        return true;
    }
    printf("issymlink: %lld is not a symlink\n", inum);
    return false;
}

int yfs_client::readlink(inum inum, std::string &dest)
{
    printf("readlink %016llx\n", inum);
    return ec->get(inum, dest);
}

int yfs_client::symlink(inum parent, const char *name, const char *link, inum &ino_out)
{
    int r = OK;
    printf("symlink name %s\n", name);

    printf("symlink link %s\n", link);
    dir dirnode;
    if ((r = getdirnode(parent, dirnode)) != OK)
        return r;

    //symbolic name exists
    bool found;
    inum found_inode;

    dirnode.lookup(name, found, found_inode);
    if (found)
        return EXIST;
    //symbolic name not exists
    if ((r = ec->create(extent_protocol::T_SYMLINK, ino_out)) != OK)
        return r;
    if ((r = ec->put(ino_out, std::string(link))) != OK)
        return r;

    dirnode.addentry(name, 0, ino_out);

    savedirnode(parent, dirnode);

    return r;
}

bool yfs_client::isdir(inum inum)
{
    // Oops! is this still correct when you implement symlink?
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR)
    {
        printf("isdir: %lld is a dir\n", inum);
        return true;
    }
    printf("isdir: %lld is not a dir\n", inum);
    return false;
}

int yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;
    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;

        printf("getfile IOERR %016llx\n", inum);
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:

    return r;
}

int yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK)
    {
        r = IOERR;

        printf("getdir IOERR %016llx\n", inum);
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}

#define EXT_RPC(xx)                                                \
    do                                                             \
    {                                                              \
        if ((xx) != extent_protocol::OK)                           \
        {                                                          \
            printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
            r = IOERR;                                             \
            goto release;                                          \
        }                                                          \
    } while (0)

// Only support set size of attr
int yfs_client::setattr(inum ino, size_t size)
{
    int r = OK;

    /*
     * your code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */

    printf("setattr %016llx\n", ino);

    std::string buf;

    if ((r = ec->get(ino, buf)) != OK)
    {
        goto release;
        return r;
    }

    if (buf.size() == size)
    {
        goto release;

        return r;
    }

    buf.resize(size, '\0');
    r = ec->put(ino, buf);
release:
    return r;
}

int yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    printf("create parent: %016llx, name: %s\n", parent, name);

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file EXIST;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    dir dirnode;
    //get lock

    lc->acquire(parent);
    int q ; //no need toclear all
    ec->refresh(parent, q);

    printf("acquire lock: %016llx, name: %s\n", parent, name);

    if ((r = getdirnode(parent, dirnode)) != OK)
    {
        goto release;
    }

    bool found;
    inum found_inode;
    dirnode.lookup(name, found, found_inode);
    if (found)
    {

        printf("found EXIST %016llx, name: %s\n", parent, name);
        int w;
        ec->flush(parent, w);
        lc->release(parent);

        return EXIST;
    }
    if ((r = ec->create(extent_protocol::T_FILE, ino_out)) != OK)
    {
        goto release;
    }

    dirnode.addentry(name, mode, ino_out);

    savedirnode(parent, dirnode);

    printf("save dirnode parent:%016llx, name: %s\n", parent, name);

release:
    int w;
    ec->flush(parent, w);
    lc->release(parent);
    printf("yfs:r%d\n", r);
    return r;
}

int yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */

    //get lock

    lc->acquire(parent);
    dir dirnode;
    if ((r = getdirnode(parent, dirnode)) != OK)
    {
        goto release;
    }

    bool found;
    inum found_inode;

    dirnode.lookup(name, found, found_inode);
    if (found)
    {

        printf("found EXIST %016llx, name: %s\n", parent, name);
        int w;
        ec->flush(parent, w);
        lc->release(parent);
        return EXIST;
    }
    if ((r = ec->create(extent_protocol::T_DIR, ino_out)) != OK)
    {
        goto release;
    }

    dirnode.addentry(name, mode, ino_out);

    savedirnode(parent, dirnode);
release:
    int w;
    ec->flush(parent, w);
    lc->release(parent);
    return r;
}

int yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;

    /*
     * your code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    printf("lookup %s\n", name);

    dir dirnode;

    //lc->acquire(parent);
    if ((r = getdirnode(parent, dirnode)) != OK)//change atime so we should update attr in the cache
    {

        //int w ;ec->flush(parent,w);lc->release(parent);
        return r;
    }

    //int w ;ec->flush(parent,w);lc->release(parent);
    dirnode.lookup(name, found, ino_out);
release:
    return r;
}

int yfs_client::readdir(inum parent, std::list<dirent> &list)
{
    int r = OK;

    lc->acquire(parent);
    printf("readdir %d\n", parent);
    /*
     * your code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */

    dir dirnode;
    if ((r = getdirnode(parent, dirnode)) != OK)
        goto release;

    dirnode.getentries(list);
release:

    int w;
    ec->flush(parent, w);
    lc->release(parent);
    return r;
}

int yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int r = OK;

    /*
     * your code goes here.
     * note: read using ec->get().
     */

    lc->acquire(ino);
    std::string buf;
    r = ec->get(ino, data);
    data.erase(0, off);

    if (data.size() > size)
        data.resize(size);

    printf("\ndata:%s", data.c_str());

    printf("\noffset:%d\n", off);
    printf("read %016llx actual size %lu\n", ino, data.size());

    lc->release(ino);

    return r;
    ;
}
int yfs_client::write(inum ino, size_t size, off_t off, const char *data,
                      size_t &bytes_written)
{
    int r = OK;

    /*
     * your code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */

    printf("write %016llx ", ino);

    lc->acquire(ino);
    std::string buf;
    if ((r = ec->get(ino, buf)) != OK)
    {

        printf("read %016llx not ok ", ino);

        int w;
        //ec->flush(ino, w);
        lc->release(ino);
        return r;
    }
    if (off + size > buf.size())
    {
        buf.resize(off + size, '\0');
    }

    buf.replace(off, size, data, size);
    if (off <= buf.size())
    {
        bytes_written = size;
    }
    else
    {
        bytes_written = off - buf.size() + size;
    }

    printf("bytes_written  %d ", bytes_written);

    buf.replace(off, size, data, size);

    if ((r = ec->put(ino, buf)) != extent_protocol::OK)
    {
        printf("write: fail to write file\n");

        int w;
        ec->flush(ino, w);
        lc->release(ino);
        return r;
    }
    printf("write %016llx finished ", ino);

    int w;
    ec->flush(ino, w);
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    int r = OK;

    /*
     * your code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    lc->acquire(parent);
    dir dirnode;
    if ((r = getdirnode(parent, dirnode)) != OK)
    {
        int w;
        ec->flush(parent, w);
        lc->release(parent);
        return r;
    }

    bool found;
    inum found_inode;

    dirnode.lookup(name, found, found_inode);
    if (found)
    {
//when a client remove also send clear cache to other client
        if ((r = ec->remove(found_inode)) != OK)
        {
            int w;
            ec->flush(parent, w);
            lc->release(parent);
            return r;
        }
        dirnode.removeentry(name);
        savedirnode(parent, dirnode);
        int w;
        ec->flush(parent, w);
        lc->release(parent);
    }
    else
    {
        r = NOENT;
        int w;
        ec->flush(parent, w);  
        lc->release(parent);
    }
    return r;
}

int yfs_client::getdirnode(inum ino, dir &dir_out)
{
    int r = OK;

    printf("getdirnode:%016llx start \n", ino);
    std::string buf;
    if (!isdir(ino))
    {
        printf("getdirnode IOERR:%016llx start \n", ino);
        return IOERR;
    }
    if ((r = ec->get(ino, buf)) != extent_protocol::OK)
        return r;

    printf("ec getdirnode:%016llx ok \n", ino);
    dir_out = dir(buf);

    return r;
}
int yfs_client::savedirnode(inum ino, const dir &dirnode)
{

    std::string buf;
    dirnode.dump(buf);
    int ret = ec->put(ino, buf);
    return ret;
}

yfs_client::dir::dir(const std::string &buf)
{
    const char *b = buf.c_str();
    uint32_t cnt = *(uint32_t *)b;
    b += sizeof(uint32_t);

    while (cnt-- > 0)
    {
        uint8_t len = *(uint8_t *)b;
        b += sizeof(uint8_t);
        std::string name(b, b + len);
        b += len;
        inum ino = *(inum *)b;
        b += sizeof(inum);
        entries[name] = ino;
    }
}

void yfs_client::dir::dump(std::string &out) const
{
    uint32_t cnt = entries.size();
    char *buf = new char[cnt * (sizeof(uint8_t) + sizeof(uint32_t) + MAX_NAME_LEN) + sizeof(uint32_t)];

    char *b = buf;
    *(uint32_t *)b = cnt;
    b += sizeof(uint32_t);

    std::map<std::string, inum> e = this->entries;
    std::map<std::string, inum>::iterator iter = e.begin();

    while (iter != e.end())
    {
        std::string name = iter->first;
        inum ino = iter->second;
        *(uint8_t *)b = name.size();
        b += sizeof(uint8_t);
        memcpy(b, name.c_str(), name.size());
        b += name.size();
        *(inum *)b = ino;
        b += sizeof(inum);

        iter++;
    }
    out = std::string(buf, b);

    delete[] buf;
}

void yfs_client::dir::lookup(const std::string &name, bool &found, inum &ino_out) const
{
    printf("_lookup %s\n", name.c_str());

    if (entries.find(name) != entries.end())
    {
        found = true;
        ino_out = entries.find(name)->second;
    }
    else
    {
        found = false;
    }
}

void yfs_client::dir::addentry(const std::string &name, mode_t mode, inum ino)
{
    entries[name] = ino;
}

void yfs_client::dir::removeentry(const std::string &name)
{
    entries.erase(name);
}

void yfs_client::dir::getentries(std::list<dirent> &entries) const
{

    std::map<std::string, inum> e = this->entries;
    std::map<std::string, inum>::iterator iter = e.begin();

    while (iter != e.end())
    {

        entries.push_back(dirent{iter->first, iter->second});
        iter++;
    }
}
