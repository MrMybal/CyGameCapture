---
id: automation
title: Assistant, terminal et contrôle MCP
description: Se connecter à une instance active, découvrir ses outils et piloter la capture avec les mêmes droits.
section: Guide utilisateur
parent: assistant
order: 10
sourceRevision: 9e33cf3a9732
---

La mise à jour des paquets 0.1.0 Bêta du 20 novembre 2026 comprend cette intégration et le manuel hors ligne complet anglais/français. Les anciens téléchargements du 6 novembre ne contiennent pas CyMCPSDK : remplacez ensemble binaires, assistant, runtime, documentation et licences. La capture fonctionne sans compte IA ; l’application n’installe ni modèle, ni environnement Python, ni moteur d’inférence.

## Se connecter à la bonne instance {#connect}

Conservez `CyGameCaptureMCP.dll` près de l’add-on ReShade et près de `CyGameCaptureOBS.dll`. Gardez `CyGameCaptureAI.exe` près de l’add-on. `CyGameCaptureCLI.exe` est le compagnon console. Unreal inclut le runtime SDK MIT et ce compagnon dans `Binaries/ThirdParty/CyMCPSDK/Win64` ; il ne dépend pas de l’assistant ReShade AGPL.

Lancez le jeu, créez une source CyGameCapture dans OBS ou ouvrez un projet Unreal avec le plugin. Chaque instance écrit un descripteur de connexion privé dans `%LOCALAPPDATA%\CyGameCapture\Automation`. Son nom comporte le composant et le PID. ReShade/OBS/Unreal inscrivent aussi son chemin dans leur journal. Ce fichier contient un jeton d’authentification : ne le publiez pas et ne le joignez pas à un signalement. Il disparaît lors d’un arrêt normal. Après un plantage, sélectionnez une nouvelle instance active.

```powershell
.\CyGameCaptureCLI.exe instances
.\CyGameCaptureCLI.exe list --instance 'C:\chemin\reshade-PID-aleatoire.json'
.\CyGameCaptureCLI.exe describe captures.start --instance 'C:\chemin\reshade-PID-aleatoire.json'
.\CyGameCaptureCLI.exe call app.status --instance 'C:\chemin\reshade-PID-aleatoire.json'
```

Les appels ont besoin du moteur de rendu, de la source OBS ou du monde Unreal actif. Le compagnon ne lance pas un jeu et ne charge pas un asset ou une scène de capture à votre place. La liste des fournisseurs, l’aide et la version fonctionnent sans interface ni instance ouverte.

## Droits et exécution {#permissions}

Le contrôle distant est **en lecture seule par défaut**. Activez-le vous-même dans les réglages de l’assistant ReShade, la propriété OBS **Autoriser le contrôle MCP de cette source**, ou la case de l’assistant Unreal / `CyGameCapture.Automation.AllowMutations 1`. Le même droit s’applique à chaque appel CLI, MCP et chat. Aucun `--confirm` ni outil ne permet d’activer son propre droit. Les boutons de proposition Show/Apply existants restent disponibles pour une décision manuelle.

Les opérations métier s’exécutent pendant la présentation ReShade, sur le tick vidéo de la source OBS ou sur le ticker du jeu Unreal. Les workers réseau et IA ne manipulent pas directement les objets GPU/moteur. Une commande attendant le tick pendant plus de 15 secondes échoue et est annulée ; elle n’est pas rejouée au retour de l’application. Une erreur peut néanmoins survenir après le début d’une action : relisez l’état avant de décider de répéter une modification.

## Opérations métier disponibles {#operations}

`describe NOM` fournit les schémas d’entrée et de sortie faisant autorité. Les identifiants et chemins de composants concernent l’instance courante. Aucun outil générique de shell, de fichiers, de chargement d’assets ou d’exécution de code n’est exposé.

| Hôte | Lectures | Opérations soumises au droit de contrôle |
| --- | --- | --- |
| ReShade | `app.status`, `buffers.list`, `buffers.inspect`, `captures.list`, `timeline.list` | `buffers.select`, `buffers.moment`, `captures.start`, `captures.stop`, `captures.alpha`, `view.set`, `inspector.filters`, `assistant.cancel` |
| Source OBS | `source.status`, `senders.list` | `source.sender`, `recording.start`, `recording.stop` |
| Unreal | `app.status`, `senders.list`, `streams.list`, `components.list` | `streams.stop_all`, `sender.start`, `sender.stop`, `sender.name`, `receiver.start`, `receiver.stop`, `receiver.sender` |

Les listes de buffers/timeline ReShade et les listes de découverte OBS/Unreal acceptent `offset` et `limit` (128 maximum). Les buffers peuvent être filtrés par largeur, hauteur et format exact. Suivez `next_offset` jusqu’à null. Le nombre de captures est limité par les slots de l’application. Les opérations de prise OBS concernent **toutes les sources CyGameCapture ayant l’enregistrement activé dans ce processus OBS**, avec leurs dossiers et encodeurs déjà configurés. Elles ne changent pas ces réglages. Les autres plugins et scènes OBS sont hors du registre.

`captures.start` propose les transformations `colour`, `depth` et `hud`, un slot supplémentaire, un buffer secondaire et des moments d’écriture. Un moment non nul nécessite D3D11. Les opérations Unreal ciblent des composants de capture existants dans les mondes ouverts : commencez par `components.list`. Un composant absent ou un moteur de rendu incompatible provoque une erreur. Les réglages, la création de scènes, les exports de fichiers, les lectures arbitraires d’images et l’édition d’assets restent dans leurs interfaces habituelles.

```powershell
.\CyGameCaptureCLI.exe call buffers.list --args '{"limit":16,"width":1280}' --instance 'C:\chemin\connexion.json'
# Après avoir choisi un vrai identifiant et activé le contrôle :
.\CyGameCaptureCLI.exe call captures.start --args '{"id":42}' --instance 'C:\chemin\connexion.json'
.\CyGameCaptureCLI.exe call captures.stop --args '{"all":true}' --instance 'C:\chemin\connexion.json'
```

`--args @C:\chemin\arguments.json` lit du JSON UTF-8 dans un fichier. Résultats/erreurs sont en JSON sur stdout ; les diagnostics vont sur stderr. Codes de sortie : 0 succès, 2 arguments/JSON/schéma incorrects, 3 droit refusé, 1 autre erreur. `--help` affiche l’aide ; `--version` renvoie les versions et la révision SDK. Les usages `CyGameCaptureAI.exe request.json`, `CyGameCaptureAI.exe --cli ...`, les options existantes des applications de test et les commandes console Unreal sont conservés.

## Discussion intégrée {#conversation}

L’assistant ReShade D3D11 conserve sa fenêtre ImGui avec le service Conversation du SDK. Choisissez Codex, Claude Code, OpenCode ou `openai-compatible` ; la sélection automatique détecte toujours les clients natifs installés. Ces agents utilisent votre connexion et leur gestion native du contexte. Pour LM Studio, l’API compatible `/v1` d’Ollama ou un autre service compatible, démarrez-le vous-même, puis indiquez son URL et l’identifiant exact du modèle. L’URL locale par défaut est `http://127.0.0.1:1234/v1` ; aucun modèle n’est téléchargé. `CyGameCaptureCLI providers --discover` détecte les profils disponibles sans afficher de secrets.

La réponse apparaît progressivement. **Annuler** interrompt la génération sans appliquer une proposition en attente ; les erreurs apparaissent dans la discussion. Les images de la frame conservent leur mécanisme de pièce jointe. Les sessions SDK complètes et le texte des échanges sont enregistrés dans `%LOCALAPPDATA%\CyGameCapture\AI\<jeu>` ; le texte est restauré à la réouverture, mais pas les anciennes propositions exécutables, dont les identifiants de buffers sont périmés. **Nouvelle discussion** archive le checkpoint SDK. Ces fichiers contiennent des images du jeu, des messages et des résultats privés.

Unreal ajoute **Help → CyGameCapture assistant**, un panneau Slate utilisant le même registre SDK. Il propose `codex` et `openai-compatible`, le streaming, l’annulation, une nouvelle conversation, l’historique restauré et une pièce jointe facultative image/texte par chemin absolu. Les clés API proviennent de `CYGC_AI_API_KEY`. Démarrez une nouvelle conversation après un changement de fournisseur/modèle/URL. Le budget du contexte API peut changer entre les tours. Le budget API est en octets sérialisés, pas en tokens : 0 conserve tout ; un budget réduit retire des tours anciens complets en préservant instructions système, tour actif et paires appel/résultat. L’historique complet reste enregistré. Un budget inférieur au tour actif produit une erreur explicite.

OBS reste un plugin source libobs natif sans dock de chat Qt supplémentaire. Utilisez un assistant MCP externe ou autorisez explicitement ses outils dans le chat ReShade. Cela évite d’ajouter un deuxième framework graphique au module OBS.

## Clients MCP externes et outils d’autres applications {#mcp}

```powershell
.\CyGameCaptureCLI.exe mcp-config --instance 'C:\chemin\connexion.json'
.\CyGameCaptureCLI.exe --mcp --instance 'C:\chemin\connexion.json'
```

La première commande affiche une configuration contenant l’exécutable et le chemin privé du descripteur ; la seconde lance un relais MCP stdio. Ce relais expose uniquement les outils. Le point HTTP local natif `/mcp` expose aussi les ressources et, avec un client compatible, les abonnements aux changements. Les deux utilisent le même registre et les droits de l’instance. Le point authentifié `/v1` conserve le catalogue complet en mode compact (`--full` pour list/describe/call/stdio). Le SDK négocie MCP 2026-07-28 et les versions historiques 2025-11-25 / 2025-06-18. Les clients stdio historiques utilisent initialize ; HTTP exige les en-têtes de protocole/session correspondants. Préférez le relais ou un client SDK aux requêtes artisanales. Les jetons sont des accès locaux privés, pas un service distant public.

Le chat Codex/API ReShade peut charger un fichier choisi dans **External tools allowlist file** (`AIExternalToolsFile`). Seuls les outils nommés sont importés avec leurs schémas exacts. Exemple pour une autre instance OBS locale :

```json
[
  {
    "connection": "C:/prive/obs-source-PID-aleatoire.json",
    "prefix": "obs",
    "tools": ["source.status", "senders.list"],
    "allow_mutations": false,
    "notifications": false
  }
]
```

Une modification nécessite l’autorisation explicite du pair, le droit de contrôle courant de l’application principale et celui du pair. Les erreurs métier distantes remontent sous une forme typée à l’assistant. Les notifications sont facultatives et nécessitent un serveur compatible. Les connexions sont limitées aux descripteurs locaux loopback choisis par l’utilisateur. Les listes d’outils pairs ne sont pas implémentées pour Claude/OpenCode dans ce panneau et produisent une erreur explicite ; leur connexion MCP principale reste disponible. Aucun serveur distant arbitraire n’est découvert silencieusement.

## Optimisations réglables séparément {#optimisations}

Ces clés ReShade sont dans `[CYGAMECAPTURE]`. Les paramètres de transport/stockage s’appliquent au démarrage de l’hôte : modifiez-les avant de relancer le jeu. Le droit de contrôle peut changer à chaud.

| Clé | Défaut | Effet |
| --- | --- | --- |
| `AutomationEnabled` | 1 | Démarrer l’hôte local authentifié |
| `AutomationAllowMutations` | 0 | Autoriser les modifications métier |
| `AutomationCompact` | 0 | Présenter cinq outils de découverte/ressources à la place du petit catalogue complet |
| `AutomationNotifications` | 1 | Publier les changements outils/ressources aux abonnements HTTP compatibles |
| `AutomationCacheTTL` | 1000 | TTL de découverte en millisecondes ; rafraîchissement explicite disponible |
| `AutomationPerformance` | 0 | Mesures JSONL privées sans contenu dans Automation |
| `AutomationResultMemoryBytes` | 8388608 | Mémoire maximale des résultats retenus |
| `AutomationResultTTL` | 300000 | Rétention des résultats en millisecondes |
| `AIEndpoint`, `AIKeyEnv` | URL locale, vide | Point compatible et variable d’environnement contenant sa clé |
| `AIContextBytes` | 0 | Budget du contexte API ; gestion native conservée pour les agents CLI |

Le mode compact fournit `cy_tools_search`, `cy_tools_describe`, `cy_tools_call`, `cy_resources_read` et `cy_resources_grep`. Chargez le schéma complet avant d’appeler un outil. Lisez `cygc://operations` et `cygc://state` par les ressources ; suivez les manifestes/URI des morceaux pour les longs textes. Les résultats dépassant 16 Kio utilisent des références privées dans la limite du quota de rétention. Suivez tous les `firstUri`/`nextUri` pendant leur validité. Une expiration échoue explicitement sans rejouer l’action. Les opérations actuelles, paginées et limitées aux métadonnées, ne justifient ni stockage disque temporaire ni accès fichiers/ripgrep : ils sont désactivés. Si la rétention est saturée, le SDK peut renvoyer le résultat complet directement au lieu de le tronquer silencieusement.

OBS propose des propriétés indépendantes pour mode compact/notifications/mesures/cache ; recréez la source pour les appliquer. Unreal expose `CyGameCapture.Automation.Compact`, `.Notifications`, `.CacheTTL` et `.Performance` au démarrage du sous-système (par exemple via la configuration de variables console du projet) ; `-CyGameCaptureNoMCP` désactive son hôte. Les résultats utilisent les limites communes de 8 Mio et cinq minutes. Les abonnements HTTP invalident le cache avant TTL ; le relais stdio ne transmet ni ressources ni abonnements : ses outils font des appels frais et `--full` reste disponible. Les modifications ne sont jamais mises en cache ni rejouées. Les mesures contiennent noms, durées, volumes, statuts, indicateurs de cache et horodatages, sans messages, arguments, résultats ni secrets.
