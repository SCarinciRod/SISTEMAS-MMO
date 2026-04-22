# SISTEMAS-MMO

Estrutura inicial para estudar namespaces em C++.

Mapeamento direto entre pasta e namespace:

- `mmo::core` -> `src/mmo/core/core.hpp`
- `mmo::net` -> `src/mmo/net/net.hpp`
- `mmo::auth` -> `src/mmo/auth/auth.hpp`
- `mmo::world` -> `src/mmo/world/world.hpp`
- `mmo::gameplay` -> `src/mmo/gameplay/gameplay.hpp`
- `mmo::server` -> `src/mmo/server/server.hpp`
- `mmo::persistence` -> `src/mmo/persistence/persistence.hpp`

O arquivo `src/mmo/mmo.hpp` apenas agrega esses headers.
O arquivo `src/main.cpp` é o ponto de entrada mínimo.
The working guide for future changes is [ARCHITECTURE.md](ARCHITECTURE.md).

## Core

O `core` deve ser a camada mais neutra do servidor. Ele não deve depender de rede, de banco ou de regras de jogo. A função dele é concentrar os conceitos que todo o resto vai reutilizar.

### Current layout

- `core/species.hpp` -> taxonomy and catalog of species.
- `core/stat.hpp` -> primary and derived stat calculations.
- `core/action.hpp` -> live action contract and action catalog.
- `core/recovery.hpp` -> action timing and recovery calculations.
- `core/status.hpp` -> status definitions and active status tables.
- `core/trigger.hpp` -> generic trigger vocabulary.
- `core/evolution_profile.hpp` -> evolution profiles per species.
- `core/evolution.hpp` -> low-level evaluator for a single rule.
- `core/entity.hpp` -> runtime entity table.
- `core/zone.hpp` -> runtime zone state.
- `core/event.hpp` -> delayed event scheduler.
- `core/runtime.hpp` -> aggregate world state.

For now these stay in `core` because there is no persistence layer or external content pipeline yet. When that appears, species and evolution profiles are the first candidates to move into a content/data layer, while runtime stays in `core`.

### 1. `types`

Define aliases for fixed-width types like `u8`, `u32`, `u64` and `Index`. This keeps sizes predictable across platforms.

### 2. `id`

Centraliza os identificadores do servidor, como `EntityId`, `SessionId` e `AccountId`. A ideia é escolher um tamanho estável agora para não precisar mudar isso quando o projeto crescer.

### 3. `time`

Define o relógio do jogo e a noção de tick. O servidor deve trabalhar com tempo monotônico e tick fixo, não com hora do sistema.

### 4. `error`

Padroniza códigos de erro. Isso evita depender de strings soltas e deixa as camadas futuras falando a mesma linguagem quando algo falhar.

### 5. `config`

Guarda constantes centrais do projeto, como nome do servidor, taxa de tick e limites globais. Isso reduz o risco de espalhar números mágicos pelo código.

### 6. `species`

Define o catálogo estático das espécies do jogo. A classificação ficou em duas camadas:

- `Family`: a base física ou estrutural da criatura.
- `Origin`: a origem, alteração ou condição especial da criatura.

Famílias principais:

- Humanoid
- Beast
- Elemental
- Mechanical
- Spirit

Origens principais:

- Natural
- Mutant
- Demonic
- Undead
- Corrupted
- Hybrid
- Arcane

Cada criatura viva referencia um `SpeciesId`, e não o `EntityId`, porque várias instâncias podem compartilhar a mesma definição.

### 7. `stat`

Calcula os atributos base e derivados que alimentam a construção da entidade. Essa camada é a ponte entre definição estática, status e valores efetivos do runtime.

### 8. `status`

Guarda o catálogo de status e as instâncias ativas por entidade. Aqui ficam os efeitos temporários, modificadores e estados que alteram o comportamento da criatura.

### 9. `trigger`

Defines the generic trigger language used by evolution profiles. This stays small and reusable so creatures do not need one-off logic.

### 10. `evolution_profile`

Groups triggers into profiles per species. This is where a species says which kinds of progression it can use, without hardcoding a monolithic rule tree.

### 11. `entity`

Guarda a tabela geral de entidades vivas e um índice por zona. Essa é a peça que te deixa localizar rapidamente quem está em qual região do mundo.

### 12. `zone`

Representa o estado de cada zona: ativa, dormindo, com jogadores presentes ou não. É aqui que você prepara a simulação para ficar leve nas áreas vazias.

### 13. `event`

Agenda eventos para depois. Isso permite que uma evolução, uma migração ou um aviso continue existindo mesmo quando a zona estiver inativa.

### 14. `evolution`

Concentra as regras que dizem quando uma criatura pode evoluir. O importante é que a regra fique separada do estado runtime.

### 15. `runtime`

Agrupa tabela de entidades, zonas e agenda de eventos. É o ponto que depois vai conversar com o loop do servidor.

### Ordem de prioridade

1. Tipos e IDs.
2. Tempo e tick.
3. Erros e estados.
4. Configurações centrais.
5. Espécies e catálogo estático.
6. Stat e status.
7. Triggers e profiles de evolução.
8. Entidades, zonas e agenda de eventos.
9. Só depois integrar `net` e `server`.

### O que evitar agora

- Misturar regras de combate dentro de `core`.
- Colocar detalhes de rede ou banco em `core`.
- Criar classes grandes cedo demais.
- Usar strings para tudo quando um alias ou código fixo resolve melhor.

### Species structure

1. `Family` says what the creature is in the broadest physical sense.
2. `Origin` says how the creature exists, changed, or was corrupted.
3. A creature can belong to one family and many origins.
4. `Spirit` belongs in `Family`, while `Mutant`, `Demonic`, `Undead`, `Corrupted`, `Hybrid` and `Arcane` fit better as `Origin`.

### Leitura recomendada

1. `id.hpp` para entender os identificadores.
2. `time.hpp` para entender o relógio monotônico.
3. `species.hpp` para ver o catálogo estático.
4. `stat.hpp` para ver os atributos e cálculos base.
5. `action.hpp` para ver o contrato da ação viva.
6. `recovery.hpp` para ver os tempos de recuperacao e animação.
7. `status.hpp` para ver os efeitos ativos.
8. `trigger.hpp` para ver os gatilhos base.
9. `evolution_profile.hpp` para ver como os gatilhos se juntam por espécie.
10. `entity.hpp` para ver o registro global.
11. `zone.hpp` para ver o particionamento.
12. `event.hpp` para ver o agendamento fora do tick ativo.
13. `evolution.hpp` para ver as regras de evolução.
14. `runtime.hpp` para ver como tudo se conecta.

The running changelog lives in [CHANGELOG.md](CHANGELOG.md).

