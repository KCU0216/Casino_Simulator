// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "ThreeCardPoker/ThreeCardPokerTypes.h"
#include "ThreeCardPokerTableActor.generated.h"

class Acasino_simulatorCharacter;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FThreeCardPokerTableChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FThreeCardPokerRoundCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FThreeCardPokerDealerHandRevealed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FThreeCardPokerCardDealt, const FBlackjackCard&, Card);

/**
 * Server-owned Three Card Poker table state — 1 dealer vs. 1 player at a time.
 *
 * Mirrors ADiceGame/ANPC_Dice's split (see NPC/NPC_Dice.h): ANPC_ThreeCardPoker spawns and owns
 * one of these, and hands it a player via SetInteractingPlayer whenever someone interacts. Unlike
 * ADiceGame this actor also holds the actual rules (deck, dealing, hand evaluation, payouts)
 * since Three Card Poker's rules are too heavy to belong on the thin NPC shell.
 *
 * Client -> server calls can't be RPCs declared directly on this actor: SetInteractingPlayer does
 * SetOwner(Player) (same trick as NPC_Dice), but the actual entry points below still route a
 * non-authority call through Acasino_simulatorCharacter's own Server RPCs (ServerPlaceThreeCardPokerAnte
 * etc.), which is what NPC_Dice::PlaceBet does too — see the comment there for why.
 */
UCLASS()
class CASINO_SIMULATOR_API AThreeCardPokerTableActor : public AActor
{
	GENERATED_BODY()

public:
	AThreeCardPokerTableActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-only. Assigns (or clears, with nullptr) who this table is currently playing with,
	 * and hands ownership to that player's connection so relevance/priority follow them. Resets
	 * any round in progress when the player changes. */
	void SetInteractingPlayer(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker")
	Acasino_simulatorCharacter* GetInteractingPlayer() const { return InteractingPlayer.Get(); }

	/** Entry point: places the mandatory Ante and, once it lands, immediately deals both hands
	 * (there's no other seat to wait for). Routes through a Server RPC on Player when called from
	 * a client, same as ANPC_Dice::PlaceBet. */
	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	bool PlaceAnte(Acasino_simulatorCharacter* Player, int32 Amount);

	/** Authoritative half of PlaceAnte. Server-only; validates Player is the one currently
	 * interacting with this table. */
	bool ExecutePlaceAnte(Acasino_simulatorCharacter* Player, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	bool PlacePairPlus(Acasino_simulatorCharacter* Player, int32 Amount);

	bool ExecutePlacePairPlus(Acasino_simulatorCharacter* Player, int32 Amount);

	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	bool PlayHand(Acasino_simulatorCharacter* Player);

	bool ExecutePlayHand(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	bool FoldHand(Acasino_simulatorCharacter* Player);

	bool ExecuteFoldHand(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	void ResetRound();

	/** Entry point: leaves the table (closes out the interaction), e.g. from a Close/Exit button in
	 * the betting UI. Same routing pattern as PlaceAnte/PlayHand/FoldHand. */
	UFUNCTION(BlueprintCallable, Category="ThreeCardPoker|Round")
	bool LeaveTable(Acasino_simulatorCharacter* Player);

	bool ExecuteLeaveTable(Acasino_simulatorCharacter* Player);

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Round")
	EThreeCardPokerRoundState GetRoundState() const { return RoundState; }

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Round")
	const TArray<FBlackjackCard>& GetPlayerCards() const { return PlayerCards; }

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Round")
	const TArray<FBlackjackCard>& GetDealerHand() const { return DealerHand; }

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Round")
	EThreeCardPokerHandResult GetLastResult() const { return LastResult; }

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Round")
	float GetDecisionRemainingTime() const;

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Rules")
	EThreeCardPokerHandRank GetHandRank(const TArray<FBlackjackCard>& Cards) const;

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Rules")
	bool IsDealerQualified() const;

	UFUNCTION(BlueprintPure, Category="ThreeCardPoker|Layout")
	UStaticMeshComponent* GetTableMesh() const { return TableMesh; }

	UPROPERTY(BlueprintAssignable, Category="ThreeCardPoker|Events")
	FThreeCardPokerTableChanged OnTableChanged;

	UPROPERTY(BlueprintAssignable, Category="ThreeCardPoker|Events")
	FThreeCardPokerCardDealt OnPlayerCardDealt;

	UPROPERTY(BlueprintAssignable, Category="ThreeCardPoker|Events")
	FThreeCardPokerCardDealt OnDealerCardDealt;

	UPROPERTY(BlueprintAssignable, Category="ThreeCardPoker|Events")
	FThreeCardPokerDealerHandRevealed OnDealerHandRevealed;

	UPROPERTY(BlueprintAssignable, Category="ThreeCardPoker|Events")
	FThreeCardPokerRoundCompleted OnRoundCompleted;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> TableRoot;

	/** The physical table prop (e.g. sm_pokertable). Assign the mesh per-Blueprint (BP_ThreeCardPokerTable). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> TableMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Layout")
	TObjectPtr<USceneComponent> DeckPoint;

	/** Shows the round result (e.g. "승리!") in-world above the table, mirroring what
	 * UThreeCardPokerBlueprintLibrary::GetThreeCardPokerResultText feeds the betting UI. Kept in
	 * sync with OnTableChanged (see UpdateResultText). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UTextRenderComponent> ResultText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Rules", meta=(ClampMin="1"))
	int32 MinAnteBet = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Rules", meta=(ClampMin="1"))
	int32 MinPairPlusBet = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Rules")
	bool bEnableAnteBonus = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Rules")
	FThreeCardPokerPairPlusPayouts PairPlusPayouts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Rules")
	FThreeCardPokerAnteBonusPayouts AnteBonusPayouts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Round", meta=(ClampMin="1.0"))
	float DefaultDecisionWindowSeconds = 20.0f;

	/** Delay between each of the 6 cards dealt at the start of a round, so they visibly go out one
	 * at a time instead of all appearing on the same frame. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ThreeCardPoker|Round", meta=(ClampMin="0.05"))
	float DealIntervalSeconds = 0.4f;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	EThreeCardPokerRoundState RoundState = EThreeCardPokerRoundState::WaitingForBet;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	TArray<FBlackjackCard> PlayerCards;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	TArray<FBlackjackCard> DealerHand;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	int32 AnteBet = 0;

	/** Set equal to AnteBet once the player chooses Play; stays 0 while undecided or if folded. */
	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	int32 PlayBet = 0;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	int32 PairPlusBet = 0;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	bool bFolded = false;

	/** True once the player has chosen Play or Fold (or was auto-folded by the decision timer). */
	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	bool bDecisionMade = false;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|State")
	EThreeCardPokerHandResult LastResult = EThreeCardPokerHandResult::None;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|Round")
	bool bDecisionWindowOpen = false;

	UPROPERTY(ReplicatedUsing=OnRep_TableState, BlueprintReadOnly, Category="ThreeCardPoker|Round")
	float DecisionWindowEndsAtServerTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="ThreeCardPoker|State")
	TArray<FBlackjackCard> Deck;

	UFUNCTION()
	void OnRep_TableState();

	/** Refreshes ResultText from the current table state. Bound to OnTableChanged in BeginPlay so it
	 * runs on the server (explicit broadcasts) and on every client (via OnRep_TableState) alike. */
	UFUNCTION()
	void UpdateResultText();

private:
	void BuildAndShuffleDeck();
	FBlackjackCard DrawCard();
	void DealCardToPlayer();
	void DealCardToDealer(bool bFaceUp);

	/** OnPlayerCardDealt/OnDealerCardDealt are plain (non-replicated) delegates, so DealCardToPlayer/
	 * DealCardToDealer route their broadcast through these instead of calling Broadcast() directly.
	 * NetMulticast makes the server-only DealNextRoundCard timer's per-card reveal actually run on
	 * every remote client too (a listen server's host was already seeing it locally, since its own
	 * broadcast fires in the same process — pure clients never got it before this). */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayerCardDealt(const FBlackjackCard& Card);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DealerCardDealt(const FBlackjackCard& Card);

	/** Same reasoning as the two above — OnDealerHandRevealed drives the dealer's face-down cards
	 * flipping face-up in BP_ThreeCardPokerTable, so it needs to actually reach remote clients too.
	 * Carries the revealed hand directly rather than relying on clients reading the (separately
	 * replicated) DealerHand property: RPC delivery and property replication aren't guaranteed to
	 * arrive in the same order, so a client could otherwise run this reveal before its local
	 * DealerHand has actually updated and briefly (or, if another update never nudges it, indefinitely)
	 * show the wrong/masked cards. */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_DealerHandRevealed(const TArray<FBlackjackCard>& RevealedHand);

	void StartRound();
	void DealNextRoundCard();
	void ClearDealingTimer();
	void StartDecisionWindow();
	void FinishDecisionWindow();
	void ScheduleDecisionWindowTimer();
	void ClearDecisionWindowTimer();
	void RevealDealerHandAndResolve();
	void Resolve();
	float GetServerWorldTimeSeconds() const;

	/** Ranks the 3 cards and returns a value where a strictly higher number always beats a lower
	 * one — encodes rank tier plus tiebreak (kicker) cards, including Ace-low (A-2-3) and
	 * Ace-high (Q-K-A) straights. Cards.Num() must be 3. */
	int32 EvaluateHandValue(const TArray<FBlackjackCard>& Cards) const;

	int32 GetPairPlusMultiplier(EThreeCardPokerHandRank Rank) const;
	int32 GetAnteBonusMultiplier(EThreeCardPokerHandRank Rank) const;

	// Server-only bookkeeping — not replicated, same as ANPC_Dice::InteractingPlayer. Clients don't
	// need this object reference; they just read the replicated hand/bet state below.
	TWeakObjectPtr<Acasino_simulatorCharacter> InteractingPlayer;

	TWeakObjectPtr<AActor> DefaultOwner;
	TArray<FBlackjackCard> ServerDealerHand;
	FTimerHandle DecisionWindowTimerHandle;

	// Counts up 0..5 across the opening deal (even = player's turn, odd = dealer's), driven by
	// DealingTimerHandle so DealNextRoundCard knows which card comes next.
	int32 NextDealingCardIndex = 0;
	FTimerHandle DealingTimerHandle;
};
