#include "AudioManager.h"
#include <iostream>

AudioManager& AudioManager::getInstance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::init() {
    if (initialized) {
        return true;
    }
    
    // Initialize audio device
    InitAudioDevice();
    
    if (!IsAudioDeviceReady()) {
        std::cerr << "Failed to initialize audio device" << std::endl;
        return false;
    }
    
    std::cout << "Audio device initialized successfully" << std::endl;
    
    // Load background music
    bgMusic = LoadMusicStream(RESOURCES_PATH "sound/bg_music.wav");
    if (!IsMusicReady(bgMusic)) {
        std::cerr << "Failed to load background music" << std::endl;
        CloseAudioDevice();
        return false;
    }
    bgMusic.looping = true;
    
    // Load button click sound
    clickSound = LoadSound(RESOURCES_PATH "sound/button.wav");
    if (!IsSoundReady(clickSound)) {
        std::cerr << "Failed to load button click sound" << std::endl;
        UnloadMusicStream(bgMusic);
        CloseAudioDevice();
        return false;
    }
    
    // Load shooting sound
    shootSound = LoadSound(RESOURCES_PATH "sound/shoot.wav");
    if (!IsSoundReady(shootSound)) {
        std::cerr << "Failed to load shooting sound" << std::endl;
        UnloadSound(clickSound);
        UnloadMusicStream(bgMusic);
        CloseAudioDevice();
        return false;
    }
    
    std::cout << "All audio resources loaded successfully" << std::endl;
    initialized = true;
    return true;
}

void AudioManager::playMusic() {
    if (!initialized || !IsMusicReady(bgMusic)) {
        return;
    }
    
    if (!IsMusicStreamPlaying(bgMusic)) {
        PlayMusicStream(bgMusic);
        std::cout << "Background music started" << std::endl;
    }
}

void AudioManager::playClick() {
    if (!initialized || !IsSoundReady(clickSound)) {
        return;
    }
    
    PlaySound(clickSound);
}

void AudioManager::playShoot() {
    if (!initialized || !IsSoundReady(shootSound)) {
        return;
    }
    
    PlaySound(shootSound);
}

void AudioManager::update() {
    if (!initialized || !IsMusicReady(bgMusic)) {
        return;
    }
    
    // Update music stream
    UpdateMusicStream(bgMusic);
}

void AudioManager::cleanup() {
    if (!initialized) {
        return;
    }
    
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
    
    // Close audio device
    CloseAudioDevice();
    
    initialized = false;
    std::cout << "Audio cleanup complete" << std::endl;
}

AudioManager::~AudioManager() {
    cleanup();
}
