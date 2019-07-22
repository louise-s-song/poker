#ifndef HANDSIMULATION_H
#include "Deck.h"
#include "HandLogic.h"

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

class HandSimulation {
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
  // Players still active in betting round
  set<int> activePlayers;
  // All pots, pots.back() is the pot that's currently being bet into
  // The rest are for players who are already all-in
  vector<int> pots;
  // potElibigility[i] represents all of the players that are capable of winning
  // pots[i]
  vector<set<int>> potEligibility;
  map<int, int> bets;
  //bets made and not yet collected into pot
  map<int, int> stacks;
  map<int, pair<Card, Card>> hands;
  //cards dealt to people
  vector<Card> board;
  bool roundDone;
  int prevBettor;
  // 0 = Preflop, 1 = Flop, 2 = Turn, 3 = River, 4 = Over
  int bettingRound;

  HandSimulation(int smallBlind, int bigBlind, int stackSize);
  bool IsValidBet(int player, int betSize);
  bool comparePlayerFinalHands(int a, int b);
  FinalHand GetFinalHand(int player);
  void initBettingRound();
  void endBettingRound();
  void Bet(int betSize);
  void Call();
  void Fold();
  bool isHandOver();
  bool isBettingRoundOver();
  bool canCheck(int player);
  bool isAllIn(int player);
  bool hasFolded(int player);
  void CollectPot();
  vector<vector<winnerData>> getWinners();
  void awardWinners();
  void initHand(vector<int>& playerStacks);
  void endHand();
  void BettingRound();
  void Showdown();
  void PlayHand();
  void PlayGame();
  string ShowTable(bool isShowdown);
private:
  void placeMoney(int player, int betSize);
  void handleEndAction();
};

#endif
#define HANDSIMULATION_H
