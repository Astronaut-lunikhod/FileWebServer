//
// Created by diode on 23-6-28.
//

#include "Config.h"

Config *Config::config_singleton_ = nullptr;
pthread_mutex_t Config::singleton_mutex_ = PTHREAD_MUTEX_INITIALIZER;

Config *Config::GetSingleton_() {
    if (nullptr == config_singleton_) {
        pthread_mutex_lock(&singleton_mutex_);
        if (nullptr == config_singleton_) {
            config_singleton_ = new Config();
        }
        pthread_mutex_unlock(&singleton_mutex_);
    }
    return config_singleton_;
}