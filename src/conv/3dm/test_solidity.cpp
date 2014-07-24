#include "solidity.h"


#include "wdb.h"


namespace
{


static inline const char *
strbool(bool value)
{
    return value ? "true" : "false";
}


static tree *
primitive_func(db_tree_state *UNUSED(tsp), const db_full_path *UNUSED(pathp),
	       rt_db_internal *ip, void *UNUSED(client_data))
{
    if (ip->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	bu_exit(1, "wrong db type");

    if (ip->idb_type != ID_BOT)
	bu_exit(1, "not a BoT");

    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(ip->idb_ptr);

    bu_log("num_faces: %d\n", static_cast<int>(bot.num_faces));
    bu_log("num_vertices: %d\n", static_cast<int>(bot.num_vertices));

    bu_log("is-closed: %s\n", strbool(bot_is_closed(&bot, false)));
    bu_log("is-closed-fan: %s\n", strbool(bot_is_closed(&bot, true)));
    bu_log("is-orientable: %s\n", strbool(bot_is_orientable(&bot)));
    bu_log("is-manifold: %s\n", strbool(bot_is_manifold(&bot)));

    exit(0);
}


}


int main(int argc, char **argv)
{
    if (argc != 3)
	bu_exit(1, "Usage: test_solidity FILE COMB_NAME\n"
		"\tWhere `COMB_NAME' is a combination with one bot.");

    bu_log("Reading from %s\n", argv[1]);

    char idbuf[132];
    rt_i *rtip = rt_dirbuild(argv[1], idbuf, sizeof(idbuf));

    if (rtip == RTI_NULL)
	bu_exit(1, "rt_dirbuild() failure");

    db_walk_tree(rtip->rti_dbip, 1, const_cast<const char **>(&argv[2]),
		 1, &rt_initial_tree_state,
		 NULL, NULL, primitive_func, NULL);

    return 0;
}
