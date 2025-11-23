---
ns: CFX
apiset: server
---
## NETWORK_SET_ENTITY_OWNER

```c
bool NETWORK_SET_ENTITY_OWNER(Entity entity, Player playerId, BOOL preventAutoMigration);
```
## Parameters
* **entity**: The entity to set the owner for.
* **playerId**: The player's server id to set the ownership.
* **preventAutoMigration**: When `true`, prevents automatic ownership migration.

## Return value
Returns whether or not the operation was successful.