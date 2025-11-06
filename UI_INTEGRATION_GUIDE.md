# Account System UI Integration Guide

## ✅ Successfully Integrated!

The account system has been fully integrated with the game UI. The game now features a complete authentication and account management interface.

## 🎮 Features Integrated

### 1. **Main Menu with Login/Register**
- **Before Login:** Players see options to login, register, or view the leaderboard as a guest
- **After Login:** Players see welcome message with their stats and can access game features

### 2. **Login Screen**
- Username and password input fields
- Real-time validation
- Error messages for invalid credentials
- Persistent session after successful login

### 3. **Registration Screen**
- Username validation (3-20 characters, alphanumeric + underscore)
- Email validation with proper format checking
- Password validation (minimum 6 characters)
- Confirm password field
- Duplicate username/email checking
- Success message redirects to login

### 4. **Account Info Screen**
- Display username and email
- Show player level and total score
- Games played, games won, and win rate statistics
- Player ranking (if available)
- Account creation date
- Refresh button to update stats

### 5. **Leaderboard**
- Top 20 players ranked by score
- Shows level, score, and win rate
- Color-coded rankings (Gold/Silver/Bronze for top 3)
- Auto-refreshes every 5 seconds
- Manual refresh button
- Accessible by both logged-in and guest users

### 6. **Session Management**
- Players remain logged in during gameplay
- Account info is cached and refreshed as needed
- Clean logout functionality that clears all session data

## 🎯 How to Use

### Starting the Game
```bash
cd build
./multyPlayer
```

### Testing the Account System

#### 1. **Create a New Account**
- Click "Register New Account" from the main menu
- Enter a username (3-20 characters)
- Enter a valid email address
- Enter a password (minimum 6 characters)
- Confirm the password
- Click "Register"
- After successful registration, you'll be redirected to the login screen

#### 2. **Login**
- Click "Login" from the main menu
- Enter your username and password
- Click "Login"
- Upon success, you'll see your account info in the main menu

#### 3. **View Account Info**
- After logging in, click "View Account Info"
- See all your stats and account details
- Click "Refresh" to update stats
- Click "Back" to return to main menu

#### 4. **View Leaderboard**
- Click "View Leaderboard" (available before and after login)
- See top players ranked by score
- Leaderboard auto-refreshes every 5 seconds
- Click "Refresh Now" for immediate update
- Click "Back" to return to main menu

#### 5. **Host or Join Game**
- After logging in, click "Host Game" or "Join Game"
- Your username will be automatically used in the game
- Your stats will be updated after each game

#### 6. **Logout**
- Click "Logout" from the main menu
- All session data will be cleared
- You'll return to the pre-login main menu

## 🔧 Technical Implementation Details

### UI State Management
The game uses a state machine with the following states:
- `MAIN_MENU` - Initial screen showing login/register options
- `LOGIN_SCREEN` - Login form
- `REGISTER_SCREEN` - Registration form
- `ACCOUNT_INFO` - User account details
- `LEADERBOARD` - Top players list
- `HOST_SERVER` - Server hosting screen (logged in users only)
- `JOIN_SERVER` - Join game screen (logged in users only)
- `IN_GAME` - Active gameplay

### Account Data Flow
```
User Input → AccountUI → AccountManager → SQLite Database
                     ↓
              SessionManager
                     ↓
              Game Layer
```

### Files Modified/Added

#### Core Account System:
- `include/gameLayer/AccountManager.h` - Account database management
- `src/gameLayer/AccountManager.cpp` - Account operations implementation
- `include/gameLayer/SessionManager.h` - Session token management
- `src/gameLayer/SessionManager.cpp` - Session operations
- `include/gameLayer/AuthenticationHandler.h` - Network authentication
- `src/gameLayer/AuthenticationHandler.cpp` - Auth packet handling

#### UI Integration:
- `include/gameLayer/AccountUI.h` - UI manager for account screens
- `src/gameLayer/AccountUI.cpp` - UI rendering and interaction
- `src/gameLayer/gameLayer.cpp` - Main game integration
- `thirdparty/glUI/include/glui/glui.h` - Added Space() function
- `thirdparty/glUI/src/glui.cpp` - Implemented Space() function

#### Database:
- `build/resources/game_accounts.db` - SQLite database (auto-created)

## 📊 Database Schema

The account system uses SQLite with the following schema:

```sql
CREATE TABLE accounts (
    username TEXT PRIMARY KEY,
    password_hash TEXT NOT NULL,
    email TEXT UNIQUE NOT NULL,
    level INTEGER DEFAULT 1,
    total_score INTEGER DEFAULT 0,
    games_played INTEGER DEFAULT 0,
    games_won INTEGER DEFAULT 0,
    win_rate REAL DEFAULT 0.0,
    ranking INTEGER DEFAULT 0,
    created_at TEXT NOT NULL,
    last_login TEXT
);
```

## 🎨 UI Color Scheme

The UI uses a modern color palette:
- **Primary** (Blue): Main buttons and headers
- **Success** (Green): Positive actions and messages
- **Error** (Red): Error messages and warnings
- **Warning** (Orange): Important information
- **Background** (Dark): Window backgrounds
- **Panel** (Dark Gray): UI panels and secondary buttons
- **White**: Regular text

## 🔒 Security Features

1. **Password Hashing**: Uses SHA-256 for secure password storage
2. **Input Validation**: All user inputs are validated before processing
3. **SQL Injection Prevention**: Uses prepared statements
4. **Session Tokens**: Each login generates a unique session token
5. **Session Timeout**: Sessions expire after inactivity (configurable)

## 📝 Status Messages

The UI provides real-time feedback:
- Success messages (green) for completed actions
- Error messages (red) for failures
- Messages auto-dismiss after 3 seconds
- Messages are context-aware for each action

## 🎮 Gameplay Integration

When a player is logged in:
1. Their username is automatically used in multiplayer games
2. Game results are saved to their account
3. Stats are updated after each game:
   - Total score increases
   - Games played counter increments
   - Win/loss record updates
   - Win rate recalculates
   - Level may increase based on score
4. Leaderboard updates to reflect new rankings

## 🐛 Debugging

To check the database directly:
```bash
cd build/resources
sqlite3 game_accounts.db

# View all accounts
SELECT * FROM accounts;

# View top players
SELECT username, level, total_score, win_rate 
FROM accounts 
ORDER BY total_score DESC 
LIMIT 10;

# Exit sqlite
.quit
```

To enable verbose logging, check the console output while running the game.

## 🚀 Next Steps (Optional Enhancements)

1. **Password Masking**: Replace InputText with a custom widget that masks passwords
2. **Password Reset**: Add forgot password functionality
3. **Email Verification**: Send verification emails on registration
4. **Profile Pictures**: Allow users to upload avatars
5. **Friends System**: Add friend requests and friend lists
6. **Chat System**: Integrated messaging between players
7. **Achievements**: Track and display player achievements
8. **Match History**: Show detailed game history
9. **Settings**: Allow users to customize UI and game settings
10. **2FA**: Add two-factor authentication for security

## 📦 Testing the Integration

Run the automated test to verify the backend:
```bash
cd build
../test_account_system.sh
```

To test the UI integration manually:
1. Start the game: `./multyPlayer`
2. Try registering a new account
3. Try logging in with wrong credentials (should fail)
4. Login with correct credentials (should succeed)
5. View your account info
6. Check the leaderboard
7. Logout and verify session is cleared

## ✨ Summary

The account system is now fully integrated with the game UI, providing a complete user experience from registration through gameplay. All features are working and ready for use. The system is modular and can be easily extended with additional features as needed.

**Status**: ✅ **COMPLETE AND FUNCTIONAL**
