#pragma once

#include <Geode/Geode.hpp>

class Bot;

class Settings {
  public:
    static Settings& get();

    geode::Mod* mod() const;

    template <class T>
    T value(std::string_view id) const {
        return mod()->getSettingValue<T>(std::string(id));
    }

    template <class T>
    T saved(std::string_view id) const {
        return mod()->getSavedValue<T>(std::string(id));
    }

    template <class T>
    bool save(std::string_view id, T&& value) const {
        return mod()->setSavedValue(std::string(id), std::forward<T>(value));
    }

    template <class T>
    void setValue(std::string_view id, T&& value) const {
        mod()->setSettingValue(std::string(id), std::forward<T>(value));
    }

    template <class T, class Callback>
    void listen(std::string_view id, Callback&& callback) const {
        geode::listenForSettingChanges<T>(std::string(id), std::forward<Callback>(callback), mod());
    }

    void applyDefaults() const;
    void loadRuntimeState(Bot& bot) const;

  private:
    Settings() = default;
};
