// ------------------------------------------------------------------
// Nstrike — serveur JSON-RPC 2.0 minimal (stdio + TCP optionnel).
// Méthodes : getbalance, sendtx, getblock, getblockcount, getgasprice,
// gettxreceipt, mine, chainstatus, deploytoken, callcontract.
// Auteur : Martial Zinsou
// ------------------------------------------------------------------
#pragma once

#include "common.hpp"
#include "json.hpp"
#include "chain.hpp"

namespace nstrike {

// Traite une requête JSON-RPC 2.0 (objet) et renvoie la réponse (objet).
// `id` est recopié (null si notification). Erreurs codes standards.
Json rpcHandle(const Json& req, Chain& chain);

// Lance une boucle d'écoute sur stdio (ligne = requête JSON).
void rpcServeStdio(Chain& chain);

// Lance un serveur TCP simple (facultatif, pour HTTP/WS futur).
// void rpcServeTcp(Chain& chain, uint16_t port);

} // namespace nstrike