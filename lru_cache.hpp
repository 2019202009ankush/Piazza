#include <iostream>
#include<string>
#include<unordered_map>
#include<list>
#include "fnv-hash/fnv.h"
#include "fnv-hash/hash_32.c"

#define MAX_CACHE_SIZE 8

using namespace std;

class lruCache
{
    // store keys of cache
    list<pair<string, string>> dq;

    // store references of key in cache
    unordered_map<string, list<pair<string, string>>::iterator> ma;

  public:
    void refer(string, string);
    void display();
    string getlist(string key);
    void dellist(string key);
    void putInSet(string key, string value);
    string getValue(string key);
    void deleteKey(string key);

};

/* Refers key x with in the LRU cache */
void lruCache::refer(string key, string value = "none")
{

    auto it = ma.find(key);
    if (it == ma.end())
    {
        // cache is full
        if (dq.size() == MAX_CACHE_SIZE)
        {
            //delete least recently used element
            auto last = dq.back();
            dq.pop_back();
            ma.erase(last.first);
        }
    }

    // present in cache
    else
    {
        auto del = ma[key];
        dq.erase(del);
    }
    // update reference
    dq.push_front(make_pair(key, value));
    ma[key] = dq.begin();
}

// display contents of cache
void lruCache::display()
{
    for (auto it = dq.begin(); it != dq.end();
         it++)
        cout << "key: " << (it->first) << " value: " << (it->second) << " ";

    cout << endl;
}

string lruCache::getlist(string key)
{
    string val;
    auto it = ma.find(key);
    if (it != ma.end())
    {
        auto valptr = ma[key];
        val = valptr->second;
    }
    else
        val = "none";
    return val;
}

void lruCache::dellist(string key)
{
    if (ma.find(key) != ma.end())
    {
        auto del = ma[key];
        ma.erase(del->first);
        dq.erase(del);
    }
}

void lruCache::putInSet(string key, string value)
{
    
    //Fnv32_t index = fnv_32_str(key, FNV1_32_INIT);
    this->refer(key, value);
}

string lruCache::getValue(string key)
{
    return this->getlist(key);
}

void lruCache::deleteKey(string key)
{
    this->dellist(key);
}