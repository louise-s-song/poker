#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <ctime>
#include "Card.h"
#include "Deck.h"

using namespace std;

string repeatString(string s, int a){
  //returns string that is string s repeated a times
  string ret;
  for(int i = 0; i < a; i++){
    ret += s;
  }
  return ret;
}

void WipeScreen(){
  //wipe terminal
  cout << repeatString("\n", 200) << endl;
}

int NumDigits(int a){
  //calculate number of digits of int
  return floor(log10(max(a, 1))) + 1;
}

enum HandRank{
  HighCard,
  Pair,
  TwoPair,
  Trips,
  Straight,
  Flush,
  FullHouse,
  Quads,
  StraightFlush,
  NoHand = -1
};

string HRtoString(int a){
  vector<string> store = {"High Card", "Pair", "Two Pair",
    "Trips", "Straight", "Flush", "Full House", "Quads",
    "Straight Flush"};
  return store[a];
}


class Player {
public:
  int id;
  string name;
};

class winnerData {
public:
  int player;
  int winnings;
};

class FinalHand {
public:
  HandRank handRank;
  vector<Card> hand;
};

class Game {
public:
  vector<Player> players;
  int smallBlind;
  int bigBlind;
  int stackSize;
  //multiples of bigblind to compute  starting stack
  int buttonLocation;
  Deck deck;
  int currentTurn;
  int currentBet;
  int betDifference;
  set<int> activePlayers;
  //players still active in betting round
  vector<int> pots;
  vector<set<int>> potEligibility;
  map<int, int> bets;
  //bets made and not yet collected into pot
  map<int, int> stacks;
  map<int, pair<Card, Card>> hands;
  //cards dealt to people
  vector<Card> board;
  bool roundDone;
  int prevBettor;
  // 0 = Preflop, 1 = Flop, 2 = Turn, 3 = River,  4 = Over
  int bettingRound;

  Game(int smallBlind, int bigBlind, int stackSize){
    this->smallBlind = smallBlind;
    this->bigBlind = bigBlind;
    this->stackSize = stackSize;
  }

  bool IsValidBet(int player, int betSize){
    //checks if bet/raise size is good. Doesn't do calls
    if(betSize - currentBet >= max(betDifference, bigBlind)
      || betSize == stacks[player] + bets[player]){
        return true;
    } else {
      return false;
    }
  }

  void initBettingRound() {
    if (bettingRound == 1) {
      deck.GetCard();

      board.push_back(deck.GetCard());
      board.push_back(deck.GetCard());
      board.push_back(deck.GetCard());
    } else if (bettingRound > 1) {
      deck.GetCard();

      board.push_back(deck.GetCard());
    }

    if (bettingRound == 0) {
      if(stacks.size() == 2){
        placeMoney(buttonLocation, smallBlind);
        placeMoney(buttonLocation + 1, bigBlind);
        currentTurn = buttonLocation;
      } else {
        placeMoney(buttonLocation + 1, smallBlind);
        placeMoney(buttonLocation + 2, bigBlind);
        currentTurn = (buttonLocation + 3) % stacks.size();
      }
      currentBet = bigBlind;
    } else {
      currentBet = 0;
      currentTurn = (buttonLocation + 1) % stacks.size();
    }

    while(hasFolded(currentTurn)) {
      currentTurn = (currentTurn + 1) % stacks.size();
    }

    betDifference = bigBlind;
    prevBettor = currentTurn;

    roundDone = false;
  }

  void endBettingRound() {
    CollectPot();
    bettingRound++;
  }

  void placeMoney(int player, int betSize) {
    player %= stacks.size();
    if(stacks[player] >= betSize - bets[player]){
      stacks[player] -= betSize - bets[player];
      bets[player] = betSize;
      betDifference = max(betSize - currentBet, betDifference);
      currentBet = betSize;
    } else {
      bets[player] += stacks[player];
      stacks[player] = 0;
      betDifference = max(bets[player] - currentBet, betDifference);
      currentBet = bets[player];
    }
  }

  void Bet(int betSize){
    if (betSize > currentBet) {
      prevBettor = currentTurn;
    }
    placeMoney(currentTurn, betSize);
    //handles mechanics of a bet
    handleEndAction();
  }

  void Call() {
    placeMoney(currentTurn, currentBet);
    handleEndAction();
  }

  void Fold() {
    activePlayers.erase(currentTurn);
    for (int i = 0; i < pots.size(); i++) {
      potEligibility[i].erase(currentTurn);
    }
    handleEndAction();
  }

  bool isHandOver() {
    return bettingRound > 3;
  }

  bool isBettingRoundOver() {
    int numNotAllIn = 0;
    for(int i : activePlayers) {
      if (stacks[i] > 0 && bets[i] < currentBet && !hasFolded(i)) {
        return false;
      }
      if (stacks[i] > 0 && !hasFolded(i)) {
        numNotAllIn++;
      }
    }
    return roundDone || numNotAllIn < 2 || activePlayers.size() < 2;
  }

  void handleEndAction() {
    do {
      currentTurn = (currentTurn + 1) % stacks.size();
    } while (hasFolded(currentTurn));

    if(currentTurn == prevBettor){
      roundDone = true;
    }
  }

  bool canCheck(int player) {
    return bets[player] >= currentBet;
  }

  bool isAllIn(int player) {
    return stacks[currentTurn] == 0;
  }

  bool hasFolded(int player) {
    return activePlayers.count(currentTurn) == 0;
  }

  void CollectPot(){
    while(true) {
      // Get the smallest bet made, so that it can be cherry picked out
      int smallestActiveBet = -1;
      for(int i = 0; i < stacks.size(); i++) {
        if (potEligibility.back().count(i)) {
          if (smallestActiveBet == -1) {
            smallestActiveBet = bets[i];
          } else {
            smallestActiveBet = min(smallestActiveBet, bets[i]);
          }
        }
      }

      // Keep track of bets that must be in separate pots
      set<int> stillNeedsCollection;

      // Cherry pick all bets out
      for (int i : potEligibility.back()) {
        pots.back() += smallestActiveBet;
        bets[i] -= smallestActiveBet;
        if (bets[i] > 0) {
          stillNeedsCollection.insert(i);
        }
      }

      // If a sidepot must be made, then make it
      // Otherwise, all bets are collected, so exit
      if (stillNeedsCollection.size() > 0) {
        pots.push_back(0);
        potEligibility.push_back(stillNeedsCollection);
      } else {
        break;
      }
    }
  }

  void BettingRound(){
    //handles a single round of betting. Ex. preflop is a single betting round.
    initBettingRound();

    while(!isBettingRoundOver()){
      if(hasFolded(currentTurn)){
        currentTurn = (currentTurn + 1) % stacks.size();
          cout << prevBettor << " " << currentTurn << endl;
      } else {
        WipeScreen();
        cout << ShowTable(false);
          cout << prevBettor << " " << currentTurn << endl;
        if(isAllIn(currentTurn)){
          Call();
        } else {
          char ans = '\0';
          if(canCheck(currentTurn)) {
            while(ans != 'c' && ans != 'b'){
              cout << "Check or Bet? c/b" << endl;
              cin >> ans;
            }
          } else {
            while(ans != 'f' && ans != 'k' && ans != 'r'){
              cout << "Fold or Call or Raise? f/c/r" << endl;
              cin >> ans;
              if (ans == 'c') {
                ans = 'k';
              }
            }
          }
          if(ans == 'c'){
            cout << "You checked." << endl;
            Call();
          } else if (ans == 'f'){
            cout << "You folded." << endl;
            Fold();
          } else if (ans == 'k'){
            cout << "You called." << endl;
            Call();
          } else if (ans == 'r' || ans == 'b'){
            int bet = 0;
            cout << "How much are you betting?" << endl;
            cin >> bet;
            while(!IsValidBet(currentTurn, bet)){
              cin.clear();
              cin.ignore();
              cout << "That is not a valid bet size." << endl;
              cin >> bet;
            }
            Bet(bet);
          }
          cin.ignore();
        }
      }
      //stops the while loop if the previous bettor is reached
      //this is the first person to go if no one bets
    }

    endBettingRound();
  }

  vector<Card> FiveCards(vector<Card> primary,
    vector<Card> remainder){
      //returns a five card hand. primary cards are
      //first, remainder fills up until there are 5 cards
      //remainder is sorted from lowest to highest
      Deck remainderDeck = Deck(remainder);
      calc: while(primary.size() < 5){
        Card temp = remainderDeck.GetCard();
        for(int i = 0; i < primary.size(); i++){
          if(primary[i].Equals(temp) ){
            goto calc;
          }
        }
        primary.push_back(temp);
      }
      return primary;
    }

    FinalHand GetFinalHand(int player) {
    //From 7 cards, calculates the best 5 card hand and
    //returns relevant cards in order
    vector<vector<Suit>> orderedRank(15);
    //vector index indicaets rank, suits of that rank are stored in the vector cells
    vector<vector<int>> orderedSuit(4);
    //vector index indicates suit, ranks of that suit are stored in the vector cells
    vector<Card> finalHand;
    finalHand.push_back(hands[player].first);
    finalHand.push_back(hands[player].second);
    if (board.size() < 3) {
      return {NoHand, finalHand};
    } else {
      for(Card& c : board) {
        finalHand.push_back(c);
      }
    }
    //has the 7 finalHand cards
    for(Card& c : finalHand){
      orderedRank[c.GetRank()].push_back(c.GetSuit());
      orderedSuit[c.GetSuit()].push_back(c.GetRank());
    }
    bool isFlush = false;
    bool isStraightFlush = false;
    vector<Card> flushhand;
    for(int i = 0; i < 4; i++){
      if(orderedSuit[i].size() >= 5){
        isFlush = true;
        sort(orderedSuit[i].begin(), orderedSuit[i].end());
        reverse(orderedSuit[i].begin(), orderedSuit[i].end());
        //largest to smallest
        for(int j = 0; j < 5; j++){
          flushhand.push_back(Card(orderedSuit[i][j], Suit(i)));
        }
        int inarow = 1;
        for(int j = 1; j < orderedSuit[i].size(); j++){
          if(orderedSuit[i][j] == orderedSuit[i][j-1] + 1){
            inarow++;
          } else {
            inarow = 1;
          }
          if(inarow >= 5){
            vector<Card> straightflush;
            for(int k = j-4; k < j+1; k++){
              straightflush.push_back(Card(orderedSuit[i][k], Suit(i)));
            }
            return {StraightFlush, straightflush};
          }
          if (inarow == 4 &&
            orderedSuit[i][orderedSuit[i].size() - 1] == 2 &&
            orderedSuit[i][0] == 14){
            vector<Card> straightflush;
            for(int k = orderedSuit[i].size() - 1;
              k >= orderedSuit[i].size() - 4; k--){
              straightflush.push_back(Card(orderedSuit[i][k], Suit(i)));
            }
            straightflush.push_back(Card(orderedSuit[i][0], Suit(i)));
            return {StraightFlush, straightflush};
          }
          //Ace-5 straight calculation
        }
      }
    }
    sort(finalHand.begin(), finalHand.end(),
      [](const Card& a, const Card& b)
      {
          return a.GetRank() < b.GetRank();
      }
    );
    //sort cards by rank from smallest to largest
    vector<int> pairs;
    //stores rank of any pairs
    vector<int> trips;
    //stores rank of any trips
    //ordered greatest to least
    for(int i = 14; i >= 2; i--){
      if(orderedRank[i].size() == 2){
        pairs.push_back(i);
      } else if (orderedRank[i].size() == 3){
        trips.push_back(i);
      } else if (orderedRank[i].size() == 4){
        vector<Card> quadhand;
        for(int j = 0; j < 4; j++){
          quadhand.push_back(Card(i, orderedRank[i][j]));
        }
        return {Quads, FiveCards(quadhand, finalHand)};
      }
    }
    if(pairs.size() >= 1 && trips.size() >= 1){
      vector<Card> fullhouse;
      for(int i = 0; i < 3; i++){
        fullhouse.push_back(Card(trips[0],
          orderedRank[trips[0]][i]));
      }
      for(int i = 0; i < 2; i++){
        fullhouse.push_back(Card(pairs[0],
          orderedRank[pairs[0]][i]));
      }
      return {FullHouse, fullhouse};
    }
    if(isFlush){
      return {Flush, flushhand};
    }
    bool isStraight = false;
    vector<int> presentRanks;
    int inarow = 1;
    for(int i = 13; i >= 2; i--){
      if(orderedRank[i].size() > 0 &&
        orderedRank[i+1].size() > 0){
          inarow++;
      } else {
        inarow = 1;
      }
      if (inarow >= 5 ){
        vector<Card> straightHand;
        for(int j = i; j < i+5; j++){
          straightHand.push_back(Card(j, orderedRank[j][0]));
        }
        return {Straight, straightHand};
      } else if (inarow >= 4 && orderedRank[14].size() >= 1 &&
        i == 2) {
          vector<Card> straightHand;
          straightHand.push_back(Card(14, orderedRank[14][0]));
          for(int j = i; j < i+4; j++){
            straightHand.push_back(Card(j, orderedRank[j][0]));
          }
          return {Straight, straightHand};
      }
    }
    if(trips.size() >= 1){
      vector<Card> tripsHand;
      for(int i = 0; i < 3; i++){
        tripsHand.push_back(Card(trips[0], orderedRank[trips[0]][i]));
      }
      return {Trips, FiveCards(tripsHand, finalHand)};
    }
    if(pairs.size() >= 2){
      vector<Card> twoPair;
      for(int i = 0; i < 2; i++){
        for(int j = 0; j < 2; j++){
          twoPair.push_back(Card(pairs[i], orderedRank[pairs[i]][j]));
        }
      }
      return {TwoPair, FiveCards(twoPair, finalHand)};
    }
    if(pairs.size() >= 1){
      vector<Card> pairHand;
      for(int i = 0; i < 2; i++){
        pairHand.push_back(Card(pairs[0], orderedRank[pairs[0]][i]));
      }
      return {Pair, FiveCards(pairHand, finalHand)};
    }
    return {HighCard, FiveCards(vector<Card>(), finalHand)};
  }

  bool compareFinalHands(const FinalHand& a, const FinalHand& b) {
    if(a.handRank == b.handRank){
      for(int i = 0; i < 7; i++){
        int aRank = a.hand[i].GetRank();
        int bRank = b.hand[i].GetRank();
        if (aRank != bRank) {
          return aRank < bRank;
        }
      }
    } else {
      return a.handRank < b.handRank;
    }
  }

  bool compareFinalHands(int a, int b) {
    return compareFinalHands(GetFinalHand(a), GetFinalHand(b));
  }

  vector<vector<winnerData>> getWinners() {
    vector<vector<winnerData>> ret;
    for( int i = 0; i < pots.size(); i++) {
      vector<winnerData> playerWinnings;

      for(int j : potEligibility[i]){
        playerWinnings.push_back({j, 0});
        //if multiple remaining players, get their hand ranks
      }

      // Sort from least to greatest
      sort(playerWinnings.begin(), playerWinnings.end(),
        [&](const winnerData& a,
           const winnerData& b)
        {
          return compareFinalHands(b.player, a.player);
        }
      );

      // Sorts playerHands from highest to lowest, equivalent to sorting by rank
      int numChopping = 1;
      for (int j = 1; j <= playerWinnings.size(); j++) {
        if (j == playerWinnings.size() || compareFinalHands(playerWinnings[j].player, playerWinnings[j-1].player)) {
          for(int k = j - numChopping; k < j; k++) {
            playerWinnings[k].winnings = pots[i] / numChopping;
          }
          break;
        } else {
          numChopping++;
        }
      }

      ret.push_back(playerWinnings);
    }

    return ret;
  }

  void Showdown(){
    //computes post-betting action, such as saying who won
    WipeScreen();
    cout << ShowTable(true) << endl;
    vector<vector<winnerData>> winners = getWinners();
    for (int i = 0; i < pots.size(); i++) {
      bool isChopping = false;
      if (winners[i].size() > 1 && winners[i][1].winnings > 1) {
        isChopping = true;
      }
      for (const winnerData& w : winners[i]) {
        if (w.winnings > 0) {
          if (isChopping) {
            cout << players[w.player].name << " chops the pot of " << pots[i] << " and wins " << w.winnings;
          } else {
            cout << players[w.player].name << " wins the pot of " << pots[i];
          }
          if(activePlayers.size() > 1) {
            cout << " with a " <<
              HRtoString(GetFinalHand(w.player).handRank) << ": " <<
              Deck(GetFinalHand(w.player).hand).ShowDeck() << endl;
          } else {
            cout << "." << endl;
          }
        } else {
          cout << players[w.player].name <<
            " has a " << HRtoString(GetFinalHand(w.player).handRank) << ", " <<
            Deck(GetFinalHand(w.player).hand).ShowDeck() << endl;
        }
      }
      cout << endl;
    }
    awardWinners();
  }

  void awardWinners() {
    vector<vector<winnerData>> winners = getWinners();
    for (int i = 0; i < pots.size(); i++) {
      for (const winnerData& w : winners[i]) {
        stacks[w.player] += w.winnings;
      }
    }
  }

  void PlayHand() {
    vector<int> newStacks;
    for(int i = 0; i < stacks.size(); i++) {
      newStacks.push_back(stacks[i]);
    }
    initHand(newStacks);

    WipeScreen();
    for (int i = 0; i < stacks.size(); i++) {
      if (stacks[i] == 0) {
        continue;
      }
      cout << "Showing " << players[i].name
        << "\'s hand. Press enter to continue." << endl;
      cin.ignore();
      WipeScreen();
      cout << players[i].name << ", " <<
        "your hand is " <<
        hands[i].first.ShowCard() << " " <<
        hands[i].second.ShowCard() << endl;
      cout << endl;
      cout << "Press enter to continue." << endl;
      cin.ignore();
      WipeScreen();
    }
    BettingRound();
    while(!isHandOver()){
      BettingRound();
    }

    Showdown();
    endHand();
  }

  void initHand( vector<int>& playerStacks ) {
    activePlayers.clear();
    stacks.clear();

    // Deal cards and initialize player data
    deck.Shuffle();
    for(int i = 0; i < playerStacks.size(); i++) {
      stacks[i] = playerStacks[i];
      if (stacks[i] > 0) {
        activePlayers.insert(i);
        Card a = deck.GetCard();
        Card b = deck.GetCard();
        hands[i] = {a, b};
      }
    }

    // Initialize game data
    pots.clear();
    pots.push_back(0);

    potEligibility.clear();
    potEligibility.push_back(activePlayers);

    bets.clear();
    board.clear();
    bettingRound = 0;
  }

  void endHand() {
    int oldButtonLocation = buttonLocation;

    buttonLocation++;
    buttonLocation %= stacks.size();
    while(stacks[buttonLocation] == 0 && buttonLocation != oldButtonLocation){
      buttonLocation++;
      buttonLocation %= stacks.size();
    }
  }

  void PlayGame(){
    buttonLocation = (rand()) % stacks.size();
    while(true){
      PlayHand();
      cout << endl << "Press enter to continue." << endl;
      cin.ignore();
      WipeScreen();
    }
    //need to update players to remove knocked out
    //players
  }

  string ShowTable(bool isShowdown){
    //creates boker board graphic which displays board, pot,
    //stacks, bets, button, and who has folded
    int longestName = 0;
    for(int i = 0; i < stacks.size(); i++){
      if(players[i].name.size() > longestName){
        longestName = players[i].name.size();
      }
    }
    int maxDigits = 0;
    for(int i = 0; i < stacks.size(); i++){
      int numDigits = NumDigits(stacks[i]);
      if(numDigits > maxDigits){
        maxDigits = numDigits;
      }
    }
    int totalWidth = 19 + longestName + maxDigits;

    string divider = "";
    for(int i = 0; i < totalWidth; i++){
      divider += "=";
    }
    divider += "|\n";

    string table = divider;
    if(board.size() > 0){
      table += Deck(board).ShowDeck();
      table += repeatString(" ", totalWidth - board.size()*3);
      table += "|\n";
    }
    for(int i = 0; i < pots.size(); i++) {
      table += "Pot: " + to_string(pots[i]) +
        repeatString(" ", totalWidth - 5 - NumDigits(pots[i])) + "|\n";
    }
    table += divider;
    for(int i = 0; i < stacks.size(); i++){
      if(i == currentTurn){
        table += "> ";
      } else {
        table += "  ";
      }
      table += players[i].name + repeatString(" ",
        2 + longestName - players[i].name.size());
      table += to_string(stacks[i]) + repeatString(" ",
        maxDigits + 1 - NumDigits(stacks[i]));
      if(i == buttonLocation){
        table += "[B] ";
      } else {
        table += "    ";
      }
      if(activePlayers.count(i) == 1 && isShowdown){
        if (activePlayers.size() > 1) {
          table += "[ " + hands[i].first.ShowCard() + " " +
            hands[i].second.ShowCard() + " ] ";
        } else {
          table += "Mucked    ";
        }
      } else if (activePlayers.count(i) == 1){
        table += to_string(bets[i]) + repeatString(" ",
        10 - NumDigits(bets[i]));
      } else {
        table += "Folded" + repeatString(" ", 4);
      }
      table += "|\n";
    }
    table += divider;
    return table;
  }
};

int main() {
  srand(time(NULL));
  int numplayers;
  cout << "How many players?" << endl;
  cin >> numplayers;
  cout << "Enter player first names." << endl;
  Game game(1, 2, 100);
  game.players = vector<Player>(numplayers);
  for(int i = 0; i < numplayers; i++){
    cin >> game.players[i].name;
    game.stacks[i] = game.bigBlind*100;
  }
  cin.ignore();
  game.PlayGame();
  return 0;
}
