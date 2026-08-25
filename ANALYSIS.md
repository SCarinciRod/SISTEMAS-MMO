# SISTEMAS-MMO Analysis

Este arquivo guarda uma leitura viva do custo do código e das decisões de arquitetura que ainda estão em aberto.

Atualize este documento quando uma mudança estrutural entrar no core.

## Snapshot atual

- `src/main.cpp` é apenas bootstrap.
- O custo real está concentrado em `src/mmo/core`.
- Catálogos estáticos ainda podem usar hash para lookup, mas a ordem e a complexidade estrutural da simulação autoritativa não dependem deles.
- Entidades, zonas e memberships usam índices ordenados; o custo alto restante vem de varreduras internas em status, combate, eventos e conteúdo realmente ativo.

## Avaliação de tempo

### Catálogos e tabelas

- `action`, `skill`, `combat`, `status`, `species` e `evolution_profile` trabalham como catálogos de conteúdo, não como índices de ordem da simulação.
- Alguns desses catálogos ainda usam hash internamente. Nenhuma decisão de gameplay depende da ordem de iteração, e as garantias do active set não assumem lookup médio `O(1)`.
- Se lookup de conteúdo aparecer como gargalo medido ou requisito adversarial, esses catálogos poderão migrar separadamente para índices ordenados ou flat maps sem alterar o contrato do `World`.

### Pontos que crescem com o conteúdo ativo

- `status::Table::apply`, `accumulate`, `sweep`, `remove_expired` e `aggregate` crescem com quantidade de status, metros e layers ativos.
- `entity::Table::refresh_record` recompõe derivados, percentuais e buffers, então tende a ser o custo mais repetido por entidade.
- `combat::AggroState::add_threat` e `select_target` crescem com o número de fontes de ameaça porque a ameaça está em vetor.
- `entity::Table::list_by_species`, `ids_in_zone`, `records_in_zone`, `species::Table::list_by_family` e `list_by_origin` são lineares no tamanho do bucket/index retornado.
- `event::Scheduler::schedule` custa $O(\log n)$ e `pop_ready` custa proporcional aos eventos prontos removidos.
- `evolution::ready` e `due_at` são essencialmente custo fixo.

## Avaliação de espaço

- O espaço cresce de forma linear com entidades, catálogos, status, ameaças e eventos pendentes.
- O modelo atual replica índices auxiliares para ganhar velocidade, então a memória cresce além da tabela principal.
- Os maiores multiplicadores de espaço hoje são:
  - status ativos por entidade;
  - shield layers e stack stages;
  - threat entries por alvo;
  - índices por zona, família, origem e espécie;
  - eventos pendentes no scheduler.

## Leitura mais funda do stat

- O pipeline atual tem duas fases claras: `stat::derive` converte o base primário em derivados e `entity::refresh_record` sobrepõe os modificadores ativos antes de recompor os recursos.
- `stat::Derived::max_weight` agora representa a capacidade de carga base para futuros cálculos de encumbrance.
- `stat::derive_encumbrance` transforma a carga atual e a capacidade máxima em bônus/penalidades de movimento e recovery, sem misturar essa regra com o resto da fórmula base.
- `entity::Inventory` guarda as instâncias vivas de item, e `entity::sync_load` já consegue transformar isso em carga atual.
- `entity::Load` guarda o peso atual da entidade para que o runtime possa compor equipamento, inventário e outras fontes de carga depois.
- Essa divisão funciona enquanto o único conjunto de modificadores vem de status, mas começa a ficar frágil quando equipamentos, passivos e bônus de conjunto entram na equação.
- O contrato de equipamento passou a separar ataque/defesa base, requisitos de uso, peso, slots de modificação, pool de affixes e instâncias vivas de affix.
- `item::calculate_inventory_weight` já oferece uma soma determinística dos pesos das instâncias para evitar lógica duplicada em camadas acima.
- `item::make_equipment_affix_instance` já formaliza o salto de regra para instância e escala o efeito com base no tier rolado.
- Hoje `item::EffectProfile` é menor do que `status::Modifiers`, então o contrato de item ainda não consegue expressar todo o espaço de bônus que o runtime já sabe consumir, mas a nova camada de affix fecha a maior lacuna.
- `experience::required_for_level` e `experience::gain` usam uma curva em degraus (patamares com saltos entre tiers) para evitar crescimento abrupto constante.
- `entity::resolve_experience_reward` garante que NPCs/monstros tenham recompensa consistente mesmo quando o blueprint não define valor explícito.
- As contas são inteiras e truncadas, então qualquer mudança de coeficiente precisa vir com exemplos de referência para evitar drift de balanceamento escondido.
- `stat::scale` zera valores não positivos; isso é aceitável para a fórmula base atual, mas não deve ser reutilizado cegamente para qualquer novo tipo de modificador negativo.
- A reformulação mais segura é separar fonte, agregação e avaliação: cada sistema gera um bloco de modificadores e o runtime só compõe e aplica o resultado final.

## Mudanças de design

### 1. Precisão e evasão removidas do núcleo

- O jogo resolve acertos por hitbox, timing e whiff, então `accuracy` e `evasion` já não fazem parte do núcleo.
- Isso simplifica a fórmula de derivados e remove uma camada probabilística que não combina com a proposta action.
- Se for útil no futuro, esses valores podem voltar como apoio de assistências leves, mas não como regra principal de acerto.

### 2. Contrato de dano e material

- O núcleo já tem os contratos base em `damage.hpp` e `material.hpp`.
- O próximo passo é plugar isso em objetos de mapa e em regras de reação, sem misturar com o combate comum.

### 3. Contrato de combate entre entidades

- O núcleo agora separa a resolução de combate entre entidades em `combat_damage.hpp`.
- Esse contrato usa dano, defesa, escudo e crítico, mas não depende de material.
- Assim, o balanceamento de entidade vs entidade fica isolado da lógica de objetos físicos do mapa.

### 4. Contrato de itens

- O núcleo agora tem `item.hpp` como contrato base para materiais, consumíveis, equipamentos, itens de quest e colecionáveis.
- O design mantém itens separados de combate e de material físico do mapa.
- Equipamentos ficam prontos para receber slots, durabilidade e bônus de set sem misturar essas regras com a física do mapa.

### 5. Interação de combate com objetos do mapa

- A ideia é boa e é possível implementar sem explodir o custo do servidor, desde que os objetos interativos sejam tratados como uma camada separada do chão/tile base.
- Tipos de dano propostos:
  - cortante
  - perfurante
  - decepante
  - impacto
  - mágico
  - elemental

- Reações propostas:
  - `ricochete`: projéteis batem em árvore/pedra e mudam de direção com dispersão limitada.
  - `entalado`: ataques físicos podem prender arma ou entidade em objeto físico do mapa por curto período.
  - `destruir`: o objeto perde durabilidade e some temporariamente quando a durabilidade chega a zero.

## Leitura de viabilidade

- Sim, é interessante.
- Sim, é possível implementar.
- O risco não é conceitual; é de custo se isso virar uma simulação ampla demais.

O caminho seguro é este:

1. Resolver a interação no momento do impacto, não em varreduras por frame.
2. Manter só objetos interativos relevantes, não todo tile do mapa.
3. Usar durabilidade, material e resistência como tabela de dados.
4. Limitar ricochete por número de saltos e energia remanescente.
5. Tratar `entalado` como estado raro, com ownership bem definido para itens presos.
6. Deixar a destruição persistente como segunda fase, se necessário.

## Ordem sugerida de implementação

1. Remoção de `accuracy` e `evasion` do núcleo de combate/estatística concluída.
2. Contrato de dano e material concluído.
3. Contrato de combate entre entidades concluído.
4. Contrato de itens concluído.
5. Adicionar durabilidade a objetos interativos do mapa.
6. Implementar `destruir` como a reação mais simples.
7. Implementar `ricochete` para projéteis.
8. Implementar `entalado` com travamento de ownership e liberação controlada.
9. Só depois ligar isso a crafting e gathering.

## Pontos de atenção

- Se todo objeto virar interativo, o custo de memória e lookup sobe rápido.
- Se `entalado` não tiver regra clara de ownership, o sistema vai gerar bugs de item preso e duplicação.
- Se ricochete não tiver limite, o combate pode ficar instável.
- Se durabilidade e resistência não forem dirigidas por dados, cada novo objeto vai virar regra manual.

## Perguntas abertas

- Que armas e skills podem gerar cada tipo de dano?
- Quais objetos do mapa serão interativos desde o início?
- `destruir` é temporário, regenerável ou permanente até o reset da zona?
- `ricochete` deve ser totalmente aleatório ou seguir um vetor de reflexão com pequena dispersão?
- `entalado` vale para monstros, NPCs e jogadores da mesma forma?

## Consolidação P1 do loop

- Antes, `server::run_loop` recalculava o peso completo de cada inventário em todo tick: custo estrutural $O(ticks \times entidades \times itens)$ mesmo sem mutação.
- Agora `Load::inventory_dirty` invalida esse valor. Spawn causa uma sincronização no primeiro tick; comandos de inventário sincronizam a carga no momento da mutação e ticks seguintes reutilizam o valor limpo. A regressão de referência é `1/0/0` para carga inicial, ticks limpos e uma mutação autoritativa.
- O bridge público `Table::mark_inventory_load_dirty(EntityId)` foi removido. `Table::find` devolve somente `const Record*`, portanto inventário vivo não pode mais ser alterado por essa rota.
- O relógio de simulação usa o índice absoluto do tick, evitando o drift de somar `1000 / tick_rate` em milissegundos. Em 60 Hz, o offset do tick 60 é exatamente 1 segundo.
- A política de atraso mantém no máximo quatro ticks vencidos por padrão e contabiliza os descartados. Tempo total/máximo de tick e tempos acumulados das fases de evento, entidade e status ficam disponíveis sem introduzir concorrência.

## P3 deterministic simulation kernel

- The P3 baseline traversal was global: every `world::step` copied all entity IDs and processed all entities before P4 introduced the sparse active set.
- The P3 canonical ordering used an `O(N log N)` sort on every tick. P4 replaced it with ordered zone/entity indexes while preserving determinism.
- Hash containers remain appropriate for lookup, but their incidental iteration order is never allowed to determine gameplay order.
- `TickContext::simulation_time` is the simulation clock used by events and statuses. Wall clock remains in `server::run_loop` only for pacing and instrumentation.
- Periodic resource mutation is owned by `entity::Table`; `mmo::world` no longer obtains a mutable `Record*` to consume status state directly.
- The kernel is single-threaded in this phase. No locks, jobs, concurrent mutation, or scheduler parallelism were introduced.
- Future optimization: authoritative zone activity semantics plus a persistent deterministic active simulation index, avoiding the global copy/sort without weakening ordering guarantees.

## P4 authoritative world and active simulation set

- Hypothesis: most known entities do not need a full tick when their zones contain no player and have no explicit wake request.
- Before: `world::step` copied every entity ID and sorted the complete list each tick, for `O(total entities)` traversal plus `O(N log N)` ordering.
- Change: `mmo::world::World` now owns spatial mutations, zone populations, event dispatch, and a persistent ordered active-zone index. Entity, zone, and zone-membership tables use `std::map`/`std::set`.
- Current hot path: iterate active zones in `ZoneId` order and their members in `EntityId` order. Traversal is `O(active zones + active entities)`; lookup and index mutation are worst-case `O(log N)` and do not rely on average-case hash behavior.
- Functional scale case: 1,024 known entities, 64 active, 960 sleeping, 100 ticks. The kernel performs 6,400 entity considerations and records 96,000 skipped considerations instead of polling 102,400 entities. The observed local duration was 11 ms in the MinGW Debug test run; this is diagnostic context, not a universal SLA.
- Tradeoff: ordered trees allocate per node and have weaker cache locality than flat storage. This increment chooses explicit ordering and worst-case guarantees; a future measured optimization may use sorted flat indexes while preserving the same contracts.
- Sleep/wake policy: due events remain global; full entity maintenance pauses in sleeping zones; absolute status deadlines remain authoritative; periodic catch-up is capped at four applications per status per step to bound wake cost.
- P5 removed the remaining live-record escape hatch; `entity::Table::find` and `World::find_entity` expose only `const Record*`.

## P5 enforced World authority boundary

- Before: callers could replace or mutate `World` entity, zone, scheduler, and output containers; `Table::find` exposed mutable live records; `adjust_health` captured wall clock; scheduling could bypass the validated World boundary; outputs had no consumption lifecycle.
- After: aggregate storage is private, reads are const queries, entity/spatial writes are named World operations, health mutation receives logical time, normal scheduling is validated, and normal/rejected outputs have ordered swap-based drains.
- No mutable live `Record*` call sites remain. Identity remains a simple data contract, but external code cannot mutate the live identity and desynchronize zone player counts or activity.
- The active-set semantics and ordered `std::map`/`std::set` indexes are unchanged. Active traversal still materializes a temporary entity-ID vector, and `entity_ids_in_zone` still copies each zone's IDs; optimize only after measurement.
- `event::Event` still represents both scheduled input and emitted output. Separating `ScheduledEvent`, `DomainEvent`, and authoritative commands belongs to the next model stage.
- `server::run_loop` still catches exceptions from a tick and advances. Allocation failures from ordered containers/vectors, status/output growth, or an external `TickObserver` can occur after part of a tick has mutated state. Continuing can therefore expose a partially advanced tick. A future policy should fail fast or mark the World faulted and recover from a known snapshot; P5 intentionally does not change loop behavior.
- Next stage: P6 deterministic authoritative command pipeline. Commands should be introduced only now that direct mutation routes are closed, otherwise a queue would order one write path while uncontrolled writes could still bypass it.
