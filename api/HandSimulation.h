#ifndef HANDSIMULATION_H
#include "Deck.h"
#include "HandLogic.h"

#include <map>
#include <set>

class winnerData {
public:
  int player;
  int winnings;
};

class HandSimulation {
public:
  HandSimulation(int bigBlind, int buttonLocation, const vector<int>& stacks);
  bool isValidBet(int player, int betSize);
  bool isValidBet(int betSize);
  bool comparePlayerFinalHands(int a, int b);
  FinalHand GetFinalHand(int player);
  void initBettingRound();
  void endBettingRound();
  void Bet(int betSize);
  void Check();
  void Call();
  void Fold();
  bool isHandOver();
  bool isBettingRoundOver();
  bool canCheck();
  bool isAllIn(int player);
  bool hasFolded(int player);
  void CollectPot();
  vector<vector<winnerData>> getWinners();
  void awardWinners();
  void endHand();
  void BettingRound();
  int numPlayers();
  int getCurrentTurn();
  int numPots();
  int getPot(int pot);
  const vector<Card>& getBoard();
  const vector<int>& getStacks();
  const vector<int>& getBets();
  const vector<int>& getPots();
  const set<int>& getActivePlayers();
  int getCurrentBet();
  int getMinRaise();
  int getBet(int player);
  bool mustShowdown();
  const pair<Card, Card>& getHand(int player);
  int getButtonLocation();
  void Showdown();
  void PlayHand();
  void PlayGame();
  string ShowTable(bool isShowdown);
private:
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
  vector<int> bets;
  //bets made and not yet collected into pot
  vector<int> stacks;
  vector<pair<Card, Card>> hands;
  //cards dealt to people
  vector<Card> board;
  bool roundDone;
  int prevBettor;
  // 0 = Preflop, 1 = Flop, 2 = Turn, 3 = River, 4 = Over
  int bettingRound;

  void placeMoney(int player, int betSize);
  void advanceAction();
};

#endif
#define HANDSIMULATION_H
