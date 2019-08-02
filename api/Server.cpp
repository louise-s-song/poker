#include "Server.h"
#include "JSON.h"
#include "APIFunctions.h"

#include <iostream>
#include <thread>

void init_server() {
  srand(time(NULL));
  vector<int> big_blinds = {4, 10, 20, 40, 100, 200, 400, 1000, 2000, 4000};
  vector<int> buy_ins = {50, 100, 200, 500, 1000, 20000, 50000, 10000, 20000, 500000};
  for(game_type& type : vector<game_type>({nlhe, plo})) {
    for(game_format& format : vector<game_format>({ring, sitngo})) {
      for(int table_size : vector<int>{2,6,9}){
        if (format == ring) {
          for(int big_blind : big_blinds){
            game_settings settings = {type, format, table_size, -1, big_blind};
            queue[settings];
          }
        } else if (format == sitngo) {
          for(int buy_in : buy_ins) {
            game_settings settings = {type, format, table_size, buy_in, -1};
            queue[settings];
          }
        } else {
          throw "No such format";
        }
      }
    }
  }
}

string get_player_id(string session_id) {
  session_id_to_player_id_mutex.lock_shared();
  string player_id = "";
  if (session_id_to_player_id.count(session_id)) {
    player_id = session_id_to_player_id[session_id];
  }
  session_id_to_player_id_mutex.unlock_shared();
  return player_id;
}

void set_player_id(string session_id, string player_id) {
  session_id_to_player_id_mutex.lock();
  all_players_mutex.lock();
  session_id_to_player_id[session_id] = player_id;
  if (!all_players.count(player_id)) {
    // Create new player
    all_players[player_id];
    player_mutexes[player_id] = new shared_mutex;
  }
  all_players_mutex.unlock();
  session_id_to_player_id_mutex.unlock();
}

int generate_int() {
  int ret;
  rand_mutex.lock();
  ret = rand();
  rand_mutex.unlock();
  return ret;
}

string generate_id() {
  return to_string(generate_int());
}

string get_games() {
  string ret = "[\n";
  all_games_mutex.lock_shared();
  bool first = true;
  for(auto g : all_games) {
    if (first) {
      first = false;
    } else {
      ret += ",\n";
    }
    ret += g.second->to_json(1);
  }
  if (!first) {
    ret += "\n";
  }
  all_games_mutex.unlock_shared();
  ret += "]\n";
  return ret;
}


string get_queues() {
  string s = "[\n";
  bool first = true;

  queue_mutex.lock_shared();
  for(auto&& [settings, player_ids] : queue){
    if (first) {
      s += "\t{\n";
    } else {
      s += ",\n\t{\n";
    }
    s += to_string_game_setting(settings, 1);
    set<string> unique_player_ids(player_ids.begin(), player_ids.end());
    s += format_json("num_players", unique_player_ids.size(), 2, false);
    s += "\t}";
    first = false;
  }
  queue_mutex.unlock_shared();
  if (!first) {
    s += "\n";
  }
  s += "]";
  return s;
}

// Removes player_id from the queue related to settings
void remove_from_queue(const string& player_id, const game_settings& settings) {
  vector<queue_entry>& entries = all_players[player_id]->queue_entries;
  int queue_entry_index = -1;
  for(int i = 0; i < entries.size(); i++) {
    bool valid_type = false;
    for(game_type& type : entries[i].types) {
      if (settings.type == type) {
        valid_type = true;
      }
    }
    bool valid_format = (entries[i].format == settings.format);
    bool valid_size = false;
    for(int table_size : entries[i].table_sizes) {
      if (settings.table_size == table_size) {
        valid_size = true;
      }
    }
    if (valid_type && valid_format && valid_size && settings.big_blind == entries[i].big_blind && settings.buy_in == entries[i].buy_in) {
      queue_entry_index = i;
      break;
    }
  }
  if (queue_entry_index == -1) {
    throw "No valid queue found";
  }
  queue_entry& entry = entries[queue_entry_index];
  for(game_type& type : entry.types) {
    for(int table_size : entry.table_sizes) {
      game_settings specific_queue_setting = {type, entry.format, table_size, entry.buy_in, entry.big_blind};
      vector<string>& player_ids = queue[specific_queue_setting];
      auto it = find(player_ids.begin(), player_ids.end(), player_id);
      player_ids.erase(it);
    }
  }
  entries.erase(entries.begin() + queue_entry_index);
}

// Moves players from the queues to existing games, and then potentially makes a new game if the queue is large enough
// queue_mutex and all_players_mutex must be locked in order to call this
void clear_queues() {
  //fill existing nonempty tables
  all_games_mutex.lock_shared();
  for(auto& elem : all_games){
    const string& game_id = elem.first;
    game_mutexes[game_id]->lock();
    Game* g = elem.second;
    game_settings& settings = g->settings;
    vector<string>& specific_queue = queue[settings];
    while( specific_queue.size() > 0
        && (g->player_ids.size() + g->waiting_players.size()) < g->settings.table_size
    ) {
      int new_player_index = -1;
      for (int i = 0; i < specific_queue.size(); i++) {
        string& player_id = specific_queue[i];
        bool already_playing = false;
        for (string& id : g->player_ids) {
          if (id == player_id) {
            already_playing = true;
          }
        }
        for (string& id : g->waiting_players) {
          if (id == player_id) {
            already_playing = true;
          }
        }
        for (string& id : g->leaving_players) {
          if (id == player_id) {
            already_playing = true;
          }
        }
        if (!already_playing) {
          new_player_index = i;
          break;
        }
      }
      if (new_player_index == -1) {
        // Everyone in the queue, is already sitting at this table
        break;
      }
      string& player_id = queue[settings][new_player_index];
      remove_from_queue(player_id, settings);
      g->waiting_players.push_back(player_id);
    }
    game_mutexes[game_id]->unlock();
  }
  all_games_mutex.unlock_shared();
  // Populating new tables
  for(auto& specific_queue : queue) {
    game_settings settings = specific_queue.first;
    if (settings.format != ring) {
      throw "We haven't coded tournaments or sitngos yet";
    }
    vector<string>& player_ids = specific_queue.second;
    // Keep trying to make new tables, if possible
    while(true){
      // Collect player_ids to potentially make a new table
      vector<string> new_table_player_ids;
      for(string& player_id : player_ids) {
        // If the table is full, we break
        if (new_table_player_ids.size() == settings.table_size) {
          break;
        }
        // A player_id might have tried to queue up multiple times, but we make sure new_table_player_ids is all unique here
        bool is_already_used = false;
        for (string& new_table_player_id : new_table_player_ids) {
          if (new_table_player_id == player_id) {
            is_already_used = true;
            break;
          }
        }
        if (!is_already_used) {
          new_table_player_ids.push_back(player_id);
        }
      }
      if (new_table_player_ids.size() <= settings.table_size / 2) {
        // New table is too small to create, so stop trying to make new tables from this specific queue
        break;
      }
      // Remove player_id from this queue, including any other queues associated with that queue_entry
      for(string& player_id : player_ids) {
        remove_from_queue(player_id, settings);
      }
      // Generate table and game
      string table_id = generate_id();
      string game_id = generate_id();
      HandSimulation hs(settings.big_blind, 0, vector<int>(new_table_player_ids.size(), 100*settings.big_blind));
      hs.initBettingRound();
      Table* table = new Table{table_id, game_id, new_table_player_ids, hs};
      Game* game = new Game(game_id, "NYC", settings, {table_id}, new_table_player_ids);

      all_tables_mutex.lock();
      all_tables[table_id] = table;
      table_mutexes[table_id] = new shared_mutex;
      all_tables_mutex.unlock();

      all_games_mutex.lock();
      all_games[game_id] = game;
      game_mutexes[game_id] = new shared_mutex;
      all_games_mutex.unlock();
    }
  }
}

string add_to_queue(const string& player_id, const queue_entry& entry) {
  string ret = "{}";
  queue_mutex.lock();

  // Checks that all possible queues for this queue_entry do exist
  for(const game_type& type : entry.types) {
    for(int table_size : entry.table_sizes) {
      game_settings settings = {type, entry.format, table_size, entry.buy_in, entry.big_blind};
      if (!queue.count(settings)) {
        ret = "";
      }
    }
  }

  // If all of them exist, then we add the player to each queue and update their queue_entries list
  if (ret != "") {
    for(const game_type& type : entry.types) {
      for(int table_size : entry.table_sizes) {
        game_settings settings = {type, entry.format, table_size, entry.buy_in, entry.big_blind};
        queue[settings].push_back(player_id);
      }
    }
    all_players_mutex.lock_shared();
    player_mutexes[player_id]->lock();
    all_players[player_id]->queue_entries.push_back(entry);
    player_mutexes[player_id]->unlock();
    all_players_mutex.unlock_shared();

    // Try to clear the queues if possible
    clear_queues();
  }

  queue_mutex.unlock();
  return ret;
}

string add_to_queue(string session_id, const vector<string>& types, string format, const vector<int>& table_sizes, int buy_in_or_big_blind) {
  //player joining game
  string player_id = get_player_id(session_id);
  if (player_id == "") {
    return error_json("Player not logged in");
  }
  string invalid_settings = error_json("A queue with those settings does not exist");

  // Verify validity of types, sizes, and formats, but does not check if a queue exists with those settings
  if (types.size() > 2 || table_sizes.size() > 3) {
    return invalid_settings;
  }
  if (types.size() == 2 && types[0] == types[1]) {
    return invalid_settings;
  }
  if (table_sizes.size() >= 2) {
    if (table_sizes[0] == table_sizes[1]) {
      return invalid_settings;
    }
    if (table_sizes.size() == 3) {
      if (table_sizes[2] == table_sizes[0] || table_sizes[2] == table_sizes[1]) {
        return invalid_settings;
      }
    }
  }
  for(const string& type : types) {
    if (!is_game_type(type)) {
      return invalid_settings;
    }
  }
  if (!is_game_format(format)) {
    return invalid_settings;
  }

  // Construct queue_entry object
  queue_entry entry;
  for(const string& type : types) {
    entry.types.push_back(to_game_type(type));
  }
  entry.format = to_game_format(format);
  entry.table_sizes = table_sizes;
  if (entry.format == ring) {
    entry.big_blind = buy_in_or_big_blind;
  } else {
    entry.buy_in = buy_in_or_big_blind;
  }

  // Add to queue
  string ret = add_to_queue(player_id, entry);

  // Check if queue existed
  if (ret == "") {
    return invalid_settings;
  } else {
    return ret;
  }
}

// requires: tables[table_id].hand_sim.isHandOver()
void start_new_hand(string table_id) {
  // Awarding money from prevous hand
  // Get buttonLocation and vector<int> stacks from previous handsim
  // Clear out any players that are in game.leaving_players
  // Add waiting_players into the game
  // Create new handsim based on previous handsim
  // break up game and add players to queue
  all_tables_mutex.lock();
  all_games_mutex.lock();
  Table* table = all_tables[table_id];
  HandSimulation* hand_sim = &table->hand_sim;
  hand_sim->awardWinners();
  int buttonLocation = hand_sim->getButtonLocation();
  vector<int> stacks = hand_sim->getStacks();
  vector<string> player_ids = table->player_ids;
  string& game_id = table->game_id;
  Game* game = all_games[game_id];
  //remove leaving_players
  for(string& player_id : game->leaving_players) {
    for(int i = 0; i < player_ids.size(); i++) {
      if (player_ids[i] == player_id) {
        player_ids.erase(player_ids.begin() + i);
        stacks.erase(stacks.begin() + i);
        i--;
      }
    }
  }
  //add waiting_players
  // TODO: Fix for tournaments
  for(string& player_id : game->waiting_players) {
    player_ids.push_back(player_id);
    stacks.push_back(100*game->settings.big_blind);
  }
  //updating table and game
  game->player_ids = player_ids;
  game->leaving_players.clear();
  game->waiting_players.clear();
  table->player_ids = player_ids;
  // TODO: dead button, dead smallBlind
  buttonLocation += 1;
  buttonLocation %= game->settings.table_size;
  if(player_ids.size() > game->settings.table_size/2){
    int big_blind = game->settings.big_blind; // game->settings.format == ring ? game->settings.big_blind : game->big_blind
    HandSimulation hs(big_blind, buttonLocation, stacks);
    hs.initBettingRound();
    table->hand_sim = move(hs);
  } else {
    for(string& player_id : player_ids) {
      game_settings& settings = game->settings;
      queue_entry entry = {{settings.type}, settings.format, {settings.table_size}, settings.big_blind, settings.buy_in};
      add_to_queue(player_id, entry);
    }
    all_games.erase(game_id);
    all_tables.erase(table_id);
    delete table;
    delete game;
    delete game_mutexes[game_id];
    delete table_mutexes[table_id];
    game_mutexes.erase(game_id);
    table_mutexes.erase(table_id);
  }
  all_games_mutex.unlock();
  all_tables_mutex.unlock();
}

string player_leave(string session_id, string game_id){
  string player_id = get_player_id(session_id);
  if (player_id == "") {
    return error_json("Player not logged in");
  }
  string ret;
  all_games_mutex.lock_shared();
  if(all_games.count(game_id) == 0){
    ret = error_json("Game does not exist.");
  } else {
    game_mutexes[game_id]->lock();
    Game* game = all_games[game_id];

    bool already_leaving = false;
    for(int i = 0; i < game->leaving_players.size(); i++){
      if(game->leaving_players[i] == player_id){
        already_leaving = true;
      }
    }

    if(already_leaving){
      ret = error_json("Player is already leaving");
    } else {
      vector<string>& player_ids = game->player_ids;
      bool is_playing = false;
      for(int i = 0; i < player_ids.size(); i++){
        if(player_ids[i] == player_id && !already_leaving){
          is_playing = true;
          player_ids.erase(player_ids.begin() + i);
          game->leaving_players.push_back(player_id);
          break;
        }
      }

      if (!is_playing){
        ret = error_json("Player is not playing");
      } else {
        ret = "{}\n";
      }
    }
    game_mutexes[game_id]->unlock();
  }
  all_games_mutex.unlock_shared();
  return ret;
}

string player_act(string session_id, string table_id, string action, int bet_size) {
  //poker action posts
  string player_id = get_player_id(session_id);
  if (player_id == "") {
    return error_json("Player not logged in");
  }
  string ret = "{}\n";
  all_tables_mutex.lock_shared();
  if (all_tables.count(table_id)) {
    table_mutexes[table_id]->lock();
    vector<string>& player_ids = all_tables[table_id]->player_ids;
    HandSimulation& hs = all_tables[table_id]->hand_sim;
    if (hs.isHandOver()) {
      ret = error_json("Hand is already over");
    } else if (player_ids[hs.getCurrentTurn()] != player_id) {
      ret = error_json("It is not your turn");
    } else {
      if (action == "raise") {
        if (bet_size <= hs.getCurrentBet()) {
          ret = error_json("Raise must be larger than the curent bet");
        } else if (!hs.isValidBet(bet_size)) {
          ret = error_json("Raise size is not valid");
        } else {
          hs.Bet(bet_size);
        }
      } else if (action == "bet") {
        if (hs.getCurrentBet() != 0) {
          ret = error_json("Bet cannot be made as a bet already exists");
        } else if (!hs.isValidBet(bet_size)) {
          ret = error_json("Bet size is not valid");
        } else {
          hs.Bet(bet_size);
        }
      } else if (action == "call") {
        if (hs.canCheck()) {
          ret = error_json("Cannot call if check is available");
        } else {
          hs.Call();
        }
      } else if (action == "check") {
        if (!hs.canCheck()) {
          ret = error_json("Cannot check, the current bet is " + to_string(hs.getCurrentBet()));
        } else {
          hs.Check();
        }
      } else if (action == "fold") {
        hs.Fold();
      } else {
        ret = error_json(action + " is not a valid action");
      }
      while(hs.isBettingRoundOver()) {
        hs.endBettingRound();
        if (hs.isHandOver()) {
          break;
        }
        hs.initBettingRound();
      }
      if (hs.isHandOver()) {
        thread t([table_id] {
          this_thread::sleep_for(chrono::milliseconds(2000));
          start_new_hand(table_id);
        });
        t.detach();
      }
    }
    table_mutexes[table_id]->unlock();
  }
  all_tables_mutex.unlock_shared();
  return ret;
}

string get_game_from_id(string game_id) {
  string response;
  all_games_mutex.lock_shared();
  if(all_games.count(game_id) > 0){
    game_mutexes[game_id]->lock_shared();
    Game& game = *all_games[game_id];
    response = game.to_json() + "\n";
    game_mutexes[game_id]->unlock_shared();
  } else {
    response = error_json("Game ID Not Found");
  }
  all_games_mutex.unlock_shared();

  return response;
};

string get_table_from_id(string table_id) {
  string response;
  all_tables_mutex.lock_shared();
  if(all_tables.count(table_id)){
    table_mutexes[table_id]->lock_shared();
    Table& thetable = *all_tables[table_id];
    response = thetable.to_json() + "\n";
    table_mutexes[table_id]->unlock_shared();
  } else {
    response = error_json("Table ID Not Found");
  }
  all_tables_mutex.unlock_shared();

  return response;
};

