#include "AudioManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

// Track if cleanup has been called to prevent double cleanup
static bool g_audioCleanupCalled = false;

// Initialize static members
std::mutex AudioManager::initMutex;
std::atomic<bool> AudioManager::isInitializing(false);

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::init() {
    // Prevent concurrent initialization attempts
    bool expected = false;
    if (!isInitializing.compare_exchange_strong(expected, true)) {
        // Another thread is already initializing, wait for it
        std::lock_guard<std::mutex> lock(initMutex);
        return initialized;
    }
    
    std::lock_guard<std::mutex> lock(initMutex);
    
    // Check again after acquiring lock (double-checked locking)
    if (initialized) {
        isInitializing = false;
        return true;
    }
    
    // Initialize audio device
    InitAudioDevice();
    
    if (!IsAudioDeviceReady()) {
        std::cerr << "Failed to initialize audio device" << std::endl;
        isInitializing = false;
        return false;
    }
    
    std::cout << "Audio device initialized successfully" << std::endl;
    
    // Load background music
    bgMusic = LoadMusicStream(RESOURCES_PATH "sound/bg_music.wav");
    if (!IsMusicReady(bgMusic)) {
        std::cerr << "Failed to load background music" << std::endl;
        CloseAudioDevice();
        isInitializing = false;
        return false;
    }
    bgMusic.looping = true;
    
    // Load button click sound
    clickSound = LoadSound(RESOURCES_PATH "sound/button.wav");
    if (!IsSoundReady(clickSound)) {
        std::cerr << "Failed to load button click sound" << std::endl;
        UnloadMusicStream(bgMusic);
        CloseAudioDevice();
        isInitializing = false;
        return false;
    }
    
    // Load shooting sound
    shootSound = LoadSound(RESOURCES_PATH "sound/shoot.wav");
    if (!IsSoundReady(shootSound)) {
        std::cerr << "Failed to load shooting sound" << std::endl;
        UnloadSound(clickSound);
        UnloadMusicStream(bgMusic);
        CloseAudioDevice();
        isInitializing = false;
        return false;
    }
    
    std::cout << "All audio resources loaded successfully" << std::endl;
    initialized = true;
    isInitializing = false;
    
    // Register cleanup handler for abrupt termination (Ctrl+C, window close, etc.)
    static bool atexitRegistered = false;
    if (!atexitRegistered) {
        std::atexit([]() {
            if (!g_audioCleanupCalled) {
                std::cout << "Emergency audio cleanup on exit..." << std::endl;
                AudioManager::getInstance().cleanup();
            }
        });
        atexitRegistered = true;
    }
    
    return true;
}

void AudioManager::playMusic() {
    if (!initialized || !IsAudioDeviceReady() || !IsMusicReady(bgMusic)) {
        return;
    }
    
    if (!IsMusicStreamPlaying(bgMusic)) {
        PlayMusicStream(bgMusic);
        std::cout << "Background music started" << std::endl;
    }
}

void AudioManager::playClick() {
    if (!initialized || !IsAudioDeviceReady() || !IsSoundReady(clickSound)) {
        return;
    }
    
    PlaySound(clickSound);
}

void AudioManager::playShoot() {
    if (!initialized || !IsAudioDeviceReady() || !IsSoundReady(shootSound)) {
        return;
    }
    
    PlaySound(shootSound);
}

void AudioManager::update() {
    if (!initialized || !IsAudioDeviceReady() || !IsMusicReady(bgMusic)) {
        return;
    }
    
    // Update music stream
    UpdateMusicStream(bgMusic);
}

void AudioManager::cleanup() {
    std::lock_guard<std::mutex> lock(initMutex);
    
    if (!initialized || g_audioCleanupCalled) {
        return;
    }
    
    // Mark cleanup as started to prevent double cleanup
    g_audioCleanupCalled = true;
    
    std::cout << "Cleaning up audio resources..." << std::endl;
    
    // Unload sounds
    if (IsSoundReady(shootSound)) {
        UnloadSound(shootSound);
    }
    
    if (IsSoundReady(clickSound)) {
        UnloadSound(clickSound);
    }
    
    // Unload music
    if (IsMusicReady(bgMusic)) {
        StopMusicStream(bgMusic);
        UnloadMusicStream(bgMusic);
    }
    
    // Close audio device only if it's ready
    if (IsAudioDeviceReady()) {
        CloseAudioDevice();
        
        // CRITICAL: Give PulseAudio mainloop thread time to fully terminate
        // Without this delay, rapid reinitialization causes "m->state == STATE_PASSIVE" assertion
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    initialized = false;
    std::cout << "Audio cleanup complete" << std::endl;
}

AudioManager::~AudioManager() {
    cleanup();
}
