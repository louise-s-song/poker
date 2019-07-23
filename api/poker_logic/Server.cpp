#include "Server.h"

using namespace std;

using namespace boost::property_tree;

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

enum game_type {
  nlhe,
  plo
};

enum game_format {
  ring,
  sitngo,
  tournament
};

struct Game {
  string id;
  game_type type;
  game_format format;
  int table_size;
  int buy_in;
  int big_blind;
  int blind_timer;
  vector<string> tables;
};

struct Table {
  string id;
  string game_id;
  vector<string> player_ids;
  HandSimulation hand_sim;

  string json_intarray(const vector<int>& array) {
    //no trailing comma
    string s = "";
    s += "[";
    if(array.size() > 0){
      for(int i = 0; i < array.size() - 1; i++){
        s += to_string(array[i]) + ", ";
      }
      s += to_string(array[array.size() - 1]);
    }
    s += "]";
    return s;
  }

  string output_table(){
    string s = "{\n";
    s += "\t\"id\": \"" + id + "\",\n";
    s += "\t\"game_id\": \"" + game_id + "\",\n";
    s += "\t\"player_ids\": [\n";
    if(player_ids.size() > 0) {
      for(int i = 0; i < player_ids.size() - 1; i++){
        s += "\t\t\"" + player_ids[i] + "\",\n";
      }
      s += "\t\t\"" + player_ids.back() + "\"\n";
    }
    s += "\t],\n";
    s += "\t\"board\": [";
    const vector<Card>& board = hand_sim.getBoard();
    if(board.size() > 0){
      for(int i = 0; i < board.size() - 1; i++){
        s += "\"" + board[i].ShowCard() + "\", ";
      }
      s += "\"" + board[board.size() - 1].ShowCard() + "\"";
    }
    s += "],\n";
    //need to implement get methods for hand_sim and change current
    //variable camelCase to snake_case
    s += "\t\"pots\": " + json_intarray(hand_sim.getPots()) + ",\n";
    s += "\t\"bets\": " + json_intarray(hand_sim.getBets()) + ",\n";
    s += "\t\"stacks\": " + json_intarray(hand_sim.getStacks()) + ",\n";
    s += "\t\"button_location\": " + to_string(hand_sim.getButtonLocation()) + ",\n";
    s += "\t\"current_turn\": " + to_string(hand_sim.getCurrentTurn()) + ",\n";
    s += "\t\"active_players\": "
      + json_intarray(vector<int>(hand_sim.getActivePlayers().begin(), hand_sim.getActivePlayers().end()))
      + ",\n";
    s += "\t\"current_bet\": " + to_string(hand_sim.getCurrentBet()) + ",\n";
    s += "\t\"min_raise\": " + to_string(hand_sim.getMinRaise()) + "\n";
    s += "}\n";
    //tick not implemented yet
    return s;
  }
};

map<string, Game*> all_games;
map<string, Table*> all_tables;
map<string, mutex*> game_mutexes;
map<string, mutex*> table_mutexes;
mutex all_games_mutex;
mutex all_tables_mutex;

int main(){
  cout << "HI2" << endl;
  HttpServer server;
  server.config.port = 8080;

  HandSimulation hs(2, 0, vector<int>(6, 200));
  hs.initBettingRound();
  all_tables["5"] = new Table {
    "5",
    "1234",
    {"a", "b", "c", "d", "e", "f"},
    hs
  };
  table_mutexes["5"] = new mutex;

  server.resource["^/api/queue$"]["POST"] = [](
    shared_ptr<HttpServer::Response> response,
    shared_ptr<HttpServer::Request> request
  ) {
  //implement table queues
};
  //return table info
  server.resource["^/api/table/([0-9]+)$"]["GET" ] = [](
    shared_ptr<HttpServer::Response> response,
    shared_ptr<HttpServer::Request> request
  ) {
    string table_id = request->path_match[1].str();
    string res;
    all_tables_mutex.lock();
    if(all_tables.count(table_id)){
      table_mutexes[table_id]->lock();
      Table& thetable = *all_tables[table_id];
      res = thetable.output_table();
      table_mutexes[table_id]->unlock();
    } else {
      res = "{\n";
      res += "\t\"error\": \"Table ID Not Found\"\n";
      res += "}\n";
    }

    *response << "HTTP/1.1 200 OK\r\n"
              << "Content-Length: " << res.size() << "\r\n\r\n"
              << res;
    all_tables_mutex.unlock();
  };
  server.start();
}