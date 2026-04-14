#include "structure.h"

#include "loggers/network_logger.h"

void ipmanipulatorDestroy(tunnel_t *t)
{
    ipmanipulator_tstate_t *state = tunnelGetState(t);

    if (state->trick_echo_sni_first_sni != NULL)
    {
        memoryFree(state->trick_echo_sni_first_sni);
    }

    tunnelDestroy(t);
}
