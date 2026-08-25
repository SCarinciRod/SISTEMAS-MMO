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
The living complexity/design analysis lives in [ANALYSIS.md](ANALYSIS.md).

## Core

O `core` deve ser a camada mais neutra do servidor. Ele não deve depender de rede, de banco ou de regras de jogo. A função dele é concentrar os conceitos que todo o resto vai reutilizar.

### Current layout

- `core/species.hpp` -> taxonomy and catalog of species.
- `core/stat.hpp` -> primary and derived stat calculations.
- `core/action.hpp` -> live action contract and action catalog.
- `core/skill.hpp` -> skill tags, gating, and skill catalog.
- `core/combat.hpp` -> combat intent, target validation, cost, and aggro contract.
- `core/numeric.hpp` -> saturating integer arithmetic used at domain boundaries.
- `core/recovery.hpp` -> action timing and recovery calculations.
- `core/status.hpp` -> status definitions and active status tables.
- `core/trigger.hpp` -> generic trigger vocabulary.
- `core/evolution_profile.hpp` -> evolution profiles per species.
- `core/evolution.hpp` -> low-level evaluator for a single rule.
- `core/entity.hpp` -> runtime entity table.
- `core/zone.hpp` -> passive zone state and ordered storage.
- `core/event.hpp` -> delayed event scheduler.
- `world/world.hpp` -> authoritative world state, spatial mutations, active-set index, event dispatch, and deterministic headless step.
- `server/loop.hpp` -> wall-clock pacing, bounded catch-up, and phase metrics around the world step.

Definitions and local calculations stay in `core`. Runtime ownership and mutations that must coordinate entities, zones, and events live in `mmo::world`.

Current combat baseline: the default action catalog is intentionally minimal and ships only `auto_attack` and `dodge` profiles.

The combat contract is kept separate from recovery so target validation, cost payment, and aggro can evolve without forcing a bootstrap rewrite.

Combat resolution now returns the final target explicitly, which makes forced-target cases like taunt easier to reason about.

Final target priority is: player intent first, then forced-target status such as `taunt`, then monster aggro threat selection.

`taunt` is modeled as a build-up control ailment, not as direct mind control.

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

O contrato já separa dois grupos:

- Negativos com build-up: `poison`, `burn`, `bleed`, `freeze`, `daze`, `silence`, `root`, `curse` e `taunt`.
- Buffs instantâneos: `shield`, `haste`, `regeneration`, `bless` e `resistant`.

Regras principais já alinhadas:

- `shield` vira buffer de vida, pode empilhar até 30% da vida máxima e bloqueia build-up enquanto estiver ativo, exceto `curse`.
- `haste` acelera movimento e ação, e é removido quando `daze` entra.
- `regeneration` melhora regen natural e cura recebida, e `burn` o cancela.
- `bless` usa 5 stacks com efeitos progressivos, culminando em imunidade e amplificação de efeitos positivos.
- `resistant` aumenta limiares de build-up negativos e acelera decay.

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
6. `skill.hpp` para ver os tags e o catálogo de skills.
7. `combat.hpp` para ver o contrato de intenção, alvo, custo e aggro.
8. `recovery.hpp` para ver os tempos de recuperacao e animação.
9. `status.hpp` para ver os efeitos ativos, including build-up control ailments like taunt.
10. `trigger.hpp` para ver os gatilhos base.
11. `evolution_profile.hpp` para ver como os gatilhos se juntam por espécie.
12. `entity.hpp` para ver o registro global.
13. `zone.hpp` para ver o particionamento.
14. `event.hpp` para ver o agendamento fora do tick ativo.
15. `evolution.hpp` para ver as regras de evolução.
16. `runtime.hpp` para ver como tudo se conecta.

The running changelog lives in [CHANGELOG.md](CHANGELOG.md).

## Build

Requisitos:

- CMake 3.20 ou mais recente;
- compilador com suporte a C++17;
- Lua apenas quando `MMO_ENABLE_LUA=ON`.

Configuração, compilação e testes em um gerador de configuração única, como Ninja:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

No Visual Studio, a configuração é selecionada durante o build e o teste:

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Opções de diagnóstico:

- `MMO_WARNINGS_AS_ERRORS=ON` trata warnings como erros;
- `MMO_ENABLE_ASAN=ON` habilita AddressSanitizer quando suportado;
- `MMO_ENABLE_UBSAN=ON` habilita UndefinedBehaviorSanitizer em GCC/Clang;
- `MMO_ENABLE_TSAN=ON` reserva a configuração de ThreadSanitizer para diagnósticos futuros;
- `MMO_ENABLE_LUA=ON` compila o loader Lua e exige uma instalação de desenvolvimento do Lua.

Targets atuais:

- `mmo_core`: contratos e implementações header-only do núcleo;
- `mmo_persistence`: implementação opcional de persistência/conteúdo;
- `mmo_server`: entrypoint mínimo do servidor;
- `mmo_unit_tests`: testes determinísticos e isolados, inicialmente para `event::Scheduler`;
- `mmo_simulation_tests`: integration tests do kernel headless com relógio lógico fixo;
- `mmo_lua_disabled_tests`: contrato do adapter indisponível, criado somente com `MMO_ENABLE_LUA=OFF`;
- `mmo_lua_integration_tests`: Lua real, conteúdo e catálogo, criado somente com `MMO_ENABLE_LUA=ON`;
- `mmo_stress_tests`: testes de integração, regressão e carga em migração incremental.

O CTest executa os testes unitários e usa `MMO_STRESS_LEVEL=smoke` na suíte de stress. Os testes podem ser filtrados por categoria:

```bash
ctest --test-dir build -L unit --output-on-failure
ctest --test-dir build -L integration --output-on-failure
ctest --test-dir build -L simulation --output-on-failure
ctest --test-dir build -L stress --output-on-failure
```

Para verificar o adapter Lua real, instale os development files do Lua e use uma pasta de build separada:

```bash
cmake -S . -B build-lua -DCMAKE_BUILD_TYPE=Debug -DMMO_ENABLE_LUA=ON -DMMO_WARNINGS_AS_ERRORS=ON
cmake --build build-lua
ctest --test-dir build-lua --output-on-failure
```

Para executar a carga padrão completa diretamente:

```bash
./build/mmo_stress_tests
```
