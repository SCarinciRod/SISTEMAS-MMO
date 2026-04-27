# SISTEMAS-MMO Analysis

Este arquivo guarda uma leitura viva do custo do código e das decisões de arquitetura que ainda estão em aberto.

Atualize este documento quando uma mudança estrutural entrar no core.

## Snapshot atual

- `src/main.cpp` é apenas bootstrap.
- O custo real está concentrado em `src/mmo/core`.
- Os catálogos usam majoritariamente `unordered_map`, então buscas e inserções são baratas em média.
- O custo alto vem de varreduras internas em status, combate, entidades, eventos e listas por índice.

## Avaliação de tempo

### Catálogos e tabelas

- `action`, `skill`, `combat`, `status`, `species` e `evolution_profile` trabalham como catálogos.
- `insert`, `find` e `erase` neles ficam em custo médio $O(1)$.
- O pior caso continua $O(n)$ se a dispersão de hash piorar, mas esse não é o caso normal do projeto.

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
