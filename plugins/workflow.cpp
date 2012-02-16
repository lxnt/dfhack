#include "Core.h"
#include "Console.h"
#include "Export.h"
#include "PluginManager.h"
#include "MiscUtils.h"

#include "modules/Materials.h"
#include "modules/Items.h"
#include "modules/Gui.h"
#include "modules/Job.h"
#include "modules/World.h"

#include "df/api.h"
#include "df/world.h"
#include "df/ui.h"
#include "df/building_workshopst.h"
#include "df/building_furnacest.h"
#include "df/job.h"
#include "df/job_item.h"
#include "df/job_list_link.h"
#include "df/dfhack_material_category.h"
#include "df/item.h"
#include "df/items_other_id.h"
#include "df/tool_uses.h"
#include "df/general_ref.h"
#include "df/general_ref_unit_workerst.h"
#include "df/general_ref_unit_holderst.h"
#include "df/general_ref_building_holderst.h"
#include "df/general_ref_contains_itemst.h"
#include "df/general_ref_contained_in_itemst.h"
#include "df/general_ref_contains_unitst.h"
#include "df/itemdef_foodst.h"
#include "df/reaction.h"
#include "df/reaction_reagent_itemst.h"
#include "df/reaction_product_itemst.h"
#include "df/plant_raw.h"
#include "df/inorganic_raw.h"
#include "df/builtin_mats.h"

using std::vector;
using std::string;
using std::endl;
using namespace DFHack;
using namespace df::enums;

using df::global::world;
using df::global::ui;
using df::global::ui_workshop_job_cursor;
using df::global::job_next_id;

/* Plugin registration */

static command_result workflow_cmd(Core *c, vector <string> & parameters);

static void init_state(Core *c);
static void cleanup_state(Core *c);

DFhackCExport const char * plugin_name ( void )
{
    return "workflow";
}

DFhackCExport command_result plugin_init (Core *c, std::vector <PluginCommand> &commands)
{
    commands.clear();
    if (!world || !ui)
        return CR_FAILURE;

    if (ui_workshop_job_cursor && job_next_id) {
        commands.push_back(
            PluginCommand(
                "workflow", "Manage control of repeat jobs.",
                workflow_cmd, false,
                "  workflow enable [option...]\n"
                "  workflow disable [option...]\n"
                "    If no options are specified, enables or disables the plugin.\n"
                "    Otherwise, enables or disables any of the following options:\n"
                "     - drybuckets: Automatically empty abandoned water buckets.\n"
                "     - auto-melt: Resume melt jobs when there are objects to melt.\n"
                "  workflow jobs\n"
                "    List workflow-controlled jobs (if in a workshop, filtered by it).\n"
                "  workflow list\n"
                "    List active constraints, and their job counts.\n"
                "  workflow count <constraint-spec> <cnt-limit> [cnt-gap]\n"
                "  workflow amount <constraint-spec> <cnt-limit> [cnt-gap]\n"
                "    Set a constraint. The first form counts each stack as only 1 item.\n"
                "  workflow unlimit <constraint-spec>\n"
                "    Delete a constraint.\n"
                "Function:\n"
                "  - When the plugin is enabled, it protects all repeat jobs from removal.\n"
                "    If they do disappear due to any cause, they are immediately re-added\n"
                "    to their workshop and suspended.\n"
                "  - In addition, when any constraints on item amounts are set, repeat jobs\n"
                "    that produce that kind of item are automatically suspended and resumed\n"
                "    as the item amount goes above or below the limit. The gap specifies how\n"
                "    much below the limit the amount has to drop before jobs are resumed;\n"
                "    this is intended to reduce the frequency of jobs being toggled.\n"
                "Constraint examples:\n"
                "  workflow amount AMMO:ITEM_AMMO_BOLTS/METAL 1000 100\n"
                "  workflow amount AMMO:ITEM_AMMO_BOLTS/WOOD,BONE 200 50\n"
                "    Keep metal bolts within 900-1000, and wood/bone within 150-200.\n"
                "  workflow count FOOD 120 30\n"
                "  workflow count DRINK 120 30\n"
                "    Keep the number of prepared food & drink stacks between 90 and 120\n"
                "  workflow count BIN 30\n"
                "  workflow count BARREL 30\n"
                "  workflow count BOX/CLOTH,SILK,YARN 30\n"
                "    Make sure there are always 25-30 empty bins/barrels/bags.\n"
                "  workflow count BAR//COAL 20\n"
                "  workflow count BAR//COPPER 30\n"
                "    Make sure there are always 15-20 coal and 25-30 copper bars.\n"
                "  workflow count POWDER_MISC/SAND 20\n"
                "  workflow count BOULDER/CLAY 20\n"
                "    Collect 15-20 sand bags and clay boulders.\n"
                "  workflow amount POWDER_MISC//MUSHROOM_CUP_DIMPLE:MILL 100 20\n"
                "    Make sure there are always 80-100 units of dimple dye.\n"
                "    In order for this to work, you have to set the material of\n"
                "    the PLANT input on the Mill Plants job to MUSHROOM_CUP_DIMPLE\n"
                "    using the 'job item-material' command.\n"
            )
        );
    }

    init_state(c);

    return CR_OK;
}

DFhackCExport command_result plugin_shutdown ( Core * c )
{
    cleanup_state(c);

    return CR_OK;
}

DFhackCExport command_result plugin_onstatechange(Core* c, state_change_event event)
{
    switch (event) {
    case SC_GAME_LOADED:
        cleanup_state(c);
        init_state(c);
        break;
    case SC_GAME_UNLOADED:
        cleanup_state(c);
        break;
    default:
        break;
    }

    return CR_OK;
}

/******************************
 * JOB STATE TRACKING STRUCTS *
 ******************************/

const int DAY_TICKS = 1200;
const int MONTH_DAYS = 28;
const int YEAR_MONTHS = 12;

struct ItemConstraint;

struct ProtectedJob {
    int id;
    int building_id;
    int tick_idx;

    static int cur_tick_idx;

    df::building *holder;
    df::job *job_copy;
    int reaction_id;

    df::job *actual_job;

    bool want_resumed;
    int resume_time, resume_delay;

    std::vector<ItemConstraint*> constraints;

public:
    ProtectedJob(df::job *job) : id(job->id)
    {
        tick_idx = cur_tick_idx;
        holder = getJobHolder(job);
        building_id = holder ? holder->id : -1;
        job_copy = cloneJobStruct(job);
        actual_job = job;
        reaction_id = -1;

        want_resumed = false;
        resume_time = 0; resume_delay = DAY_TICKS;
    }

    ~ProtectedJob()
    {
        deleteJobStruct(job_copy);
    }

    bool isActuallyResumed() {
        return actual_job && !actual_job->flags.bits.suspend;
    }
    bool isResumed() {
        return want_resumed || isActuallyResumed();
    }
    bool isLive() { return actual_job != NULL; }

    void update(df::job *job)
    {
        actual_job = job;
        if (*job == *job_copy)
            return;

        reaction_id = -1;
        deleteJobStruct(job_copy);
        job_copy = cloneJobStruct(job);
    }

    void tick_job(df::job *job, int ticks)
    {
        tick_idx = cur_tick_idx;
        actual_job = job;

        if (isActuallyResumed())
        {
            resume_time = 0;
            resume_delay = std::max(DAY_TICKS, resume_delay - ticks);
        }
        else if (want_resumed)
        {
            if (!resume_time)
                want_resumed = false;
            else if (world->frame_counter >= resume_time)
                actual_job->flags.bits.suspend = false;
        }
    }

    void recover(df::job *job)
    {
        actual_job = job;
        job->flags.bits.repeat = true;
        job->flags.bits.suspend = true;

        resume_delay = std::min(DAY_TICKS*MONTH_DAYS, 5*resume_delay/3);
        resume_time = world->frame_counter + resume_delay;
    }

    void set_resumed(bool resume)
    {
        if (resume)
        {
            if (world->frame_counter >= resume_time)
                actual_job->flags.bits.suspend = false;
        }
        else
        {
            resume_time = 0;
            if (isActuallyResumed())
                resume_delay = DAY_TICKS;

            actual_job->flags.bits.suspend = true;
        }

        want_resumed = resume;
    }
};

int ProtectedJob::cur_tick_idx = 0;

typedef std::map<std::pair<int,int>, bool> TMaterialCache;

struct ItemConstraint {
    PersistentDataItem config;

    ItemTypeInfo item;

    MaterialInfo material;
    df::dfhack_material_category mat_mask;

    int weight;
    std::vector<ProtectedJob*> jobs;

    int item_amount, item_count, item_inuse;
    bool request_suspend, request_resume;

    bool is_active, cant_resume_reported;

    TMaterialCache material_cache;

public:
    ItemConstraint()
        : weight(0), item_amount(0), item_count(0), item_inuse(0)
        , is_active(false), cant_resume_reported(false)
    {}

    int goalCount() { return config.ival(0); }
    void setGoalCount(int v) { config.ival(0) = v; }

    int goalGap() {
        int gcnt = std::max(1, goalCount()/2);
        return std::min(gcnt, config.ival(1) <= 0 ? 5 : config.ival(1));
    }
    void setGoalGap(int v) { config.ival(1) = v; }

    bool goalByCount() { return config.ival(2) & 1; }
    void setGoalByCount(bool v) {
        if (v)
            config.ival(2) |= 1;
        else
            config.ival(2) &= ~1;
    }

    void init(const std::string &str)
    {
        config.val() = str;
        config.ival(2) = 0;
    }

    void computeRequest()
    {
        int size = goalByCount() ? item_count : item_amount;
        request_resume = (size <= goalCount()-goalGap());
        request_suspend = (size >= goalCount());
    }
};

/******************************
 *      GLOBAL VARIABLES      *
 ******************************/

static bool enabled = false;
static PersistentDataItem config;

static int last_tick_frame_count = 0;
static int last_frame_count = 0;

enum ConfigFlags {
    CF_ENABLED = 1,
    CF_DRYBUCKETS = 2,
    CF_AUTOMELT = 4
};

typedef std::map<int, ProtectedJob*> TKnownJobs;
static TKnownJobs known_jobs;

static std::vector<ProtectedJob*> pending_recover;
static std::vector<ItemConstraint*> constraints;

static int meltable_count = 0;
static bool melt_active = false;

/******************************
 *       MISC FUNCTIONS       *
 ******************************/

static ProtectedJob *get_known(int id)
{
    TKnownJobs::iterator it = known_jobs.find(id);
    return (it != known_jobs.end()) ? it->second : NULL;
}

static bool isSupportedJob(df::job *job)
{
    return job->misc_links.empty() &&
           getJobHolder(job) &&
           (!job->job_items.empty() ||
            job->job_type == job_type::CollectClay ||
            job->job_type == job_type::CollectSand);
}

static void enumLiveJobs(std::map<int, df::job*> &rv)
{
    df::job_list_link *p = world->job_list.next;
    for (; p; p = p->next)
        rv[p->item->id] = p->item;
}

static bool isOptionEnabled(unsigned flag)
{
    return config.isValid() && (config.ival(0) & flag) != 0;
}

static void setOptionEnabled(ConfigFlags flag, bool on)
{
    if (!config.isValid())
        return;

    if (on)
        config.ival(0) |= flag;
    else
        config.ival(0) &= ~flag;
}

/******************************
 *    STATE INITIALIZATION    *
 ******************************/

static void stop_protect(Core *c)
{
    pending_recover.clear();

    if (!known_jobs.empty())
        c->con.print("Unprotecting %d jobs.\n", known_jobs.size());

    for (TKnownJobs::iterator it = known_jobs.begin(); it != known_jobs.end(); ++it)
        delete it->second;

    known_jobs.clear();
}

static void cleanup_state(Core *c)
{
    config = PersistentDataItem();

    stop_protect(c);

    for (size_t i = 0; i < constraints.size(); i++)
        delete constraints[i];
    constraints.clear();
}

static void check_lost_jobs(Core *c, int ticks);
static ItemConstraint *get_constraint(Core *c, const std::string &str, PersistentDataItem *cfg = NULL);

static void start_protect(Core *c)
{
    check_lost_jobs(c, 0);

    if (!known_jobs.empty())
        c->con.print("Protecting %d jobs.\n", known_jobs.size());
}

static void init_state(Core *c)
{
    config = c->getWorld()->GetPersistentData("workflow/config");
    if (config.isValid() && config.ival(0) == -1)
        config.ival(0) = 0;

    enabled = isOptionEnabled(CF_ENABLED);

    // Parse constraints
    std::vector<PersistentDataItem> items;
    c->getWorld()->GetPersistentData(&items, "workflow/constraints");

    for (int i = items.size()-1; i >= 0; i--) {
        if (get_constraint(c, items[i].val(), &items[i]))
            continue;

        c->con.printerr("Lost constraint %s\n", items[i].val().c_str());
        c->getWorld()->DeletePersistentData(items[i]);
    }

    last_tick_frame_count = world->frame_counter;
    last_frame_count = world->frame_counter;

    if (!enabled)
        return;

    start_protect(c);
}

static void enable_plugin(Core *c)
{
    if (!config.isValid())
    {
        config = c->getWorld()->AddPersistentData("workflow/config");
        config.ival(0) = 0;
    }

    setOptionEnabled(CF_ENABLED, true);
    enabled = true;
    c->con << "Enabling the plugin." << endl;

    start_protect(c);
}

/******************************
 *     JOB AUTO-RECOVERY      *
 ******************************/

static void forget_job(Core *c, ProtectedJob *pj)
{
    known_jobs.erase(pj->id);
    delete pj;
}

static bool recover_job(Core *c, ProtectedJob *pj)
{
    if (pj->isLive())
        return true;

    // Check that the building exists
    pj->holder = df::building::find(pj->building_id);
    if (!pj->holder)
    {
        c->con.printerr("Forgetting job %d (%s): holder building lost.",
                        pj->id, ENUM_KEY_STR(job_type, pj->job_copy->job_type));
        forget_job(c, pj);
        return true;
    }

    // Check its state and postpone or cancel if invalid
    if (pj->holder->jobs.size() >= 10)
    {
        c->con.printerr("Forgetting job %d (%s): holder building has too many jobs.",
                        pj->id, ENUM_KEY_STR(job_type, pj->job_copy->job_type));
        forget_job(c, pj);
        return true;
    }

    if (!pj->holder->jobs.empty())
    {
        df::job_type ftype = pj->holder->jobs[0]->job_type;
        if (ftype == job_type::DestroyBuilding)
            return false;

        if (ENUM_ATTR(job_type,type,ftype) == job_type_class::StrangeMood)
            return false;
    }

    // Create and link in the actual job structure
    df::job *recovered = cloneJobStruct(pj->job_copy);

    if (!linkJobIntoWorld(recovered, false)) // reuse same id
    {
        deleteJobStruct(recovered);

        c->con.printerr("Inconsistency: job %d (%s) already in list.",
                        pj->id, ENUM_KEY_STR(job_type, pj->job_copy->job_type));
        return true;
    }

    pj->holder->jobs.push_back(recovered);

    // Done
    pj->recover(recovered);
    return true;
}

static void check_lost_jobs(Core *c, int ticks)
{
    ProtectedJob::cur_tick_idx++;
    if (ticks < 0) ticks = 0;

    df::job_list_link *p = world->job_list.next;
    for (; p; p = p->next)
    {
        df::job *job = p->item;

        ProtectedJob *pj = get_known(job->id);
        if (pj)
        {
            if (!job->flags.bits.repeat)
                forget_job(c, pj);
            else
                pj->tick_job(job, ticks);
        }
        else if (job->flags.bits.repeat && isSupportedJob(job))
        {
            pj = new ProtectedJob(job);
            assert(pj->holder);
            known_jobs[pj->id] = pj;
        }
    }

    for (TKnownJobs::const_iterator it = known_jobs.begin(); it != known_jobs.end(); ++it)
    {
        if (it->second->tick_idx == ProtectedJob::cur_tick_idx ||
            !it->second->isLive())
            continue;

        it->second->actual_job = NULL;
        pending_recover.push_back(it->second);
    }
}

static void update_job_data(Core *c)
{
    df::job_list_link *p = world->job_list.next;
    for (; p; p = p->next)
    {
        ProtectedJob *pj = get_known(p->item->id);
        if (!pj)
            continue;
        pj->update(p->item);
    }
}

static void recover_jobs(Core *c)
{
    for (int i = pending_recover.size()-1; i >= 0; i--)
        if (recover_job(c, pending_recover[i]))
            vector_erase_at(pending_recover, i);
}

static void process_constraints(Core *c);

DFhackCExport command_result plugin_onupdate(Core* c)
{
    if (!enabled)
        return CR_OK;

    // Every 5 frames check the jobs for disappearance
    static unsigned cnt = 0;
    if ((++cnt % 5) != 0)
        return CR_OK;

    check_lost_jobs(c, world->frame_counter - last_tick_frame_count);
    last_tick_frame_count = world->frame_counter;

    // Proceed every in-game half-day, or when jobs to recover changed
    static unsigned last_rlen = 0;
    bool check_time = (world->frame_counter - last_frame_count) >= DAY_TICKS/2;

    if (pending_recover.size() != last_rlen || check_time)
    {
        recover_jobs(c);
        last_rlen = pending_recover.size();

        // If the half-day passed, proceed to update
        if (check_time)
        {
            last_frame_count = world->frame_counter;

            update_job_data(c);
            process_constraints(c);
        }
    }

    return CR_OK;
}

/******************************
 *   ITEM COUNT CONSTRAINT    *
 ******************************/

static ItemConstraint *get_constraint(Core *c, const std::string &str, PersistentDataItem *cfg)
{
    std::vector<std::string> tokens;
    split_string(&tokens, str, "/");

    if (tokens.size() > 3)
        return NULL;

    int weight = 0;

    ItemTypeInfo item;
    if (!item.find(tokens[0]) || !item.isValid()) {
        c->con.printerr("Cannot find item type: %s\n", tokens[0].c_str());
        return NULL;
    }

    if (item.subtype >= 0)
        weight += 10000;

    df::dfhack_material_category mat_mask;
    std::string maskstr = vector_get(tokens,1);
    if (!maskstr.empty() && !parseJobMaterialCategory(&mat_mask, maskstr)) {
        c->con.printerr("Cannot decode material mask: %s\n", maskstr.c_str());
        return NULL;
    }

    if (mat_mask.whole != 0)
        weight += 100;

    MaterialInfo material;
    std::string matstr = vector_get(tokens,2);
    if (!matstr.empty() && (!material.find(matstr) || !material.isValid())) {
        c->con.printerr("Cannot find material: %s\n", matstr.c_str());
        return NULL;
    }

    if (material.type >= 0)
        weight += (material.index >= 0 ? 5000 : 1000);

    if (mat_mask.whole && material.isValid() && !material.matches(mat_mask)) {
        c->con.printerr("Material %s doesn't match mask %s\n", matstr.c_str(), maskstr.c_str());
        return NULL;
    }

    for (size_t i = 0; i < constraints.size(); i++)
    {
        ItemConstraint *ct = constraints[i];
        if (ct->item == item && ct->material == material &&
            ct->mat_mask.whole == mat_mask.whole)
            return ct;
    }

    ItemConstraint *nct = new ItemConstraint;
    nct->item = item;
    nct->material = material;
    nct->mat_mask = mat_mask;
    nct->weight = weight;

    if (cfg)
        nct->config = *cfg;
    else
    {
        nct->config = c->getWorld()->AddPersistentData("workflow/constraints");
        nct->init(str);
    }

    constraints.push_back(nct);
    return nct;
}

static void delete_constraint(Core *c, ItemConstraint *cv)
{
    int idx = linear_index(constraints, cv);
    if (idx >= 0)
        vector_erase_at(constraints, idx);

    c->getWorld()->DeletePersistentData(cv->config);
    delete cv;
}

/******************************
 *   JOB-CONSTRAINT MAPPING   *
 ******************************/

static void link_job_constraint(ProtectedJob *pj, df::item_type itype, int16_t isubtype,
                                df::dfhack_material_category mat_mask,
                                int16_t mat_type, int32_t mat_index)
{
    MaterialInfo mat(mat_type, mat_index);

    for (size_t i = 0; i < constraints.size(); i++)
    {
        ItemConstraint *ct = constraints[i];

        if (ct->item.type != itype ||
            (ct->item.subtype != -1 && ct->item.subtype != isubtype))
            continue;
        if (!mat.matches(ct->material))
            continue;
        if (ct->mat_mask.whole)
        {
            if (mat.isValid())
            {
                if (!mat.matches(ct->mat_mask))
                    continue;
            }
            else
            {
                if (!(mat_mask.whole & ct->mat_mask.whole))
                    continue;
            }
        }

        if (linear_index(pj->constraints, ct) >= 0)
            continue;

        ct->jobs.push_back(pj);
        pj->constraints.push_back(ct);

        if (!ct->is_active && pj->isResumed())
            ct->is_active = true;
    }
}

static void compute_custom_job(ProtectedJob *pj, df::job *job)
{
    if (pj->reaction_id < 0)
        pj->reaction_id = linear_index(df::reaction::get_vector(),
                                       &df::reaction::code, job->reaction_name);

    df::reaction *r = df::reaction::find(pj->reaction_id);
    if (!r)
        return;

    for (size_t i = 0; i < r->products.size(); i++)
    {
        using namespace df::enums::reaction_product_item_flags;

        VIRTUAL_CAST_VAR(prod, df::reaction_product_itemst, r->products[i]);
        if (!prod || prod->item_type < 0)
            continue;

        MaterialInfo mat(prod);
        df::dfhack_material_category mat_mask(0);

        bool get_mat_prod = prod->flags.is_set(GET_MATERIAL_PRODUCT);
        if (get_mat_prod || prod->flags.is_set(GET_MATERIAL_SAME))
        {
            int reagent_idx = linear_index(r->reagents, &df::reaction_reagent::code,
                                           prod->get_material.reagent_code);
            if (reagent_idx < 0)
                continue;

            int item_idx = linear_index(job->job_items, &df::job_item::reagent_index, reagent_idx);
            if (item_idx >= 0)
                mat.decode(job->job_items[item_idx]);
            else
            {
                VIRTUAL_CAST_VAR(src, df::reaction_reagent_itemst, r->reagents[reagent_idx]);
                if (!src)
                    continue;
                mat.decode(src);
            }

            if (get_mat_prod)
            {
                std::string code = prod->get_material.product_code;

                if (mat.isValid())
                {
                    int idx = linear_index(mat.material->reaction_product.id, code);
                    if (idx < 0)
                        continue;

                    mat.decode(mat.material->reaction_product.material, idx);
                }
                else
                {
                    if (code == "SOAP_MAT")
                        mat_mask.bits.soap = true;
                }
            }
        }

        link_job_constraint(pj, prod->item_type, prod->item_subtype,
                            mat_mask, mat.type, mat.index);
    }
}

static void guess_job_material(df::job *job, MaterialInfo &mat, df::dfhack_material_category &mat_mask)
{
    using namespace df::enums::job_type;

    if (job->job_type == PrepareMeal)
        mat.decode(-1);
    else
        mat.decode(job);

    mat_mask.whole = job->material_category.whole;

    // Material from the job enum
    const char *job_material = ENUM_ATTR(job_type, material, job->job_type);
    if (job_material)
    {
        MaterialInfo info;
        if (info.findBuiltin(job_material))
            mat = info;
        else
            parseJobMaterialCategory(&mat_mask, job_material);
    }

    // Material from the job reagent
    if (!mat.isValid() && !job->job_items.empty() &&
        (job->job_items.size() == 1 ||
         job->job_items[0]->item_type == item_type::PLANT))
    {
        mat.decode(job->job_items[0]);

        switch (job->job_items[0]->item_type)
        {
        case item_type::WOOD:
            mat_mask.bits.wood = mat_mask.bits.wood2 = true;
            break;
        }
    }
}

static void compute_job_outputs(Core *c, ProtectedJob *pj)
{
    using namespace df::enums::job_type;

    // Custom reactions handled in another function
    df::job *job = pj->job_copy;

    if (job->job_type == CustomReaction)
    {
        compute_custom_job(pj, job);
        return;
    }

    // Item type & subtype
    df::item_type itype = ENUM_ATTR(job_type, item, job->job_type);
    int16_t isubtype = job->item_subtype;
    if (itype == item_type::NONE)
        return;

    // Item material & material category
    MaterialInfo mat;
    df::dfhack_material_category mat_mask;
    guess_job_material(job, mat, mat_mask);

    // Job-specific code
    switch (job->job_type)
    {
    case SmeltOre:
        if (mat.inorganic)
        {
            std::vector<int16_t> &ores = mat.inorganic->metal_ore.mat_index;
            for (size_t i = 0; i < ores.size(); i++)
                link_job_constraint(pj, item_type::BAR, -1, 0, 0, ores[i]);
        }
        return;

    case ExtractMetalStrands:
        if (mat.inorganic)
        {
            std::vector<int16_t> &threads = mat.inorganic->thread_metal.mat_index;
            for (size_t i = 0; i < threads.size(); i++)
                link_job_constraint(pj, item_type::THREAD, -1, 0, 0, threads[i]);
        }
        return;

    case PrepareMeal:
        if (job->mat_type != -1)
        {
            std::vector<df::itemdef_foodst*> &food = df::itemdef_foodst::get_vector();
            for (size_t i = 0; i < food.size(); i++)
                if (food[i]->level == job->mat_type)
                    link_job_constraint(pj, item_type::FOOD, i, 0, -1, -1);
            return;
        }
        break;

#define PLANT_PROCESS_MAT(flag, tag) \
        if (mat.plant && mat.plant->flags.is_set(plant_raw_flags::flag)) \
            mat.decode(mat.plant->material_defs.type_##tag, \
                       mat.plant->material_defs.idx_##tag); \
        else mat.decode(-1);
    case MillPlants:
        PLANT_PROCESS_MAT(MILL, mill);
        break;
    case ProcessPlants:
        PLANT_PROCESS_MAT(THREAD, thread);
        break;
    case ProcessPlantsBag:
        PLANT_PROCESS_MAT(LEAVES, leaves);
        break;
    case ProcessPlantsBarrel:
        PLANT_PROCESS_MAT(EXTRACT_BARREL, extract_barrel);
        break;
    case ProcessPlantsVial:
        PLANT_PROCESS_MAT(EXTRACT_VIAL, extract_vial);
        break;
    case ExtractFromPlants:
        PLANT_PROCESS_MAT(EXTRACT_STILL_VIAL, extract_still_vial);
        break;
#undef PLANT_PROCESS_MAT

    default:
        break;
    }

    link_job_constraint(pj, itype, isubtype, mat_mask, mat.type, mat.index);
}

static void map_job_constraints(Core *c)
{
    melt_active = false;

    for (size_t i = 0; i < constraints.size(); i++)
    {
        constraints[i]->jobs.clear();
        constraints[i]->is_active = false;
    }

    for (TKnownJobs::const_iterator it = known_jobs.begin(); it != known_jobs.end(); ++it)
    {
        ProtectedJob *pj = it->second;

        pj->constraints.clear();

        if (!pj->isLive())
            continue;

        if (!melt_active && pj->actual_job->job_type == job_type::MeltMetalObject)
            melt_active = pj->isResumed();

        compute_job_outputs(c, pj);
    }
}

/******************************
 *  ITEM-CONSTRAINT MAPPING   *
 ******************************/

static void dryBucket(df::item *item)
{
    for (size_t i = 0; i < item->itemrefs.size(); i++)
    {
        df::general_ref *ref = item->itemrefs[i];
        if (ref->getType() == general_ref_type::CONTAINS_ITEM)
        {
            df::item *obj = ref->getItem();

            if (obj &&
                obj->getType() == item_type::LIQUID_MISC &&
                obj->getMaterial() == builtin_mats::WATER)
            {
                obj->flags.bits.garbage_colect = true;
                obj->flags.bits.hidden = true;
            }
        }
    }
}

static bool itemBusy(df::item *item)
{
    using namespace df::enums::item_type;

    for (size_t i = 0; i < item->itemrefs.size(); i++)
    {
        df::general_ref *ref = item->itemrefs[i];
        if (ref->getType() == general_ref_type::CONTAINS_ITEM)
        {
            df::item *obj = ref->getItem();
            if (obj && !obj->flags.bits.garbage_colect)
                return true;
        }
        else if (ref->getType() == general_ref_type::CONTAINS_UNIT)
            return true;
        else if (ref->getType() == general_ref_type::UNIT_HOLDER)
        {
            if (!item->flags.bits.in_job)
                return true;
        }
        else if (ref->getType() == general_ref_type::CONTAINED_IN_ITEM)
        {
            df::item *obj = ref->getItem();
            if (!obj)
                continue;

            // Stuff in flasks and backpacks is busy
            df::item_type type = obj->getType();
            if ((type == FLASK && item->getType() == DRINK) || type == BACKPACK)
                return true;
        }
    }

    return false;
}

static bool itemInRealJob(df::item *item)
{
    if (!item->flags.bits.in_job)
        return false;

    if (item->jobs.size() != 1 ||
        item->jobs[0]->unk1 != 2 ||
        item->jobs[0]->job == NULL)
        return true;

    return ENUM_ATTR(job_type, type, item->jobs[0]->job->job_type)
               != job_type_class::Hauling;
}

static void map_job_items(Core *c)
{
    for (size_t i = 0; i < constraints.size(); i++)
    {
        constraints[i]->item_amount = 0;
        constraints[i]->item_count = 0;
        constraints[i]->item_inuse = 0;
    }

    meltable_count = 0;

    // Precompute a bitmask with the bad flags
    df::item_flags bad_flags;
    bad_flags.whole = 0;

#define F(x) bad_flags.bits.x = true;
    F(dump); F(forbid); F(garbage_colect);
    F(hostile); F(on_fire); F(rotten); F(trader);
    F(in_building); F(construction); F(artifact1);
#undef F

    bool dry_buckets = isOptionEnabled(CF_DRYBUCKETS);

    std::vector<df::item*> &items = world->items.other[items_other_id::ANY_FREE];

    for (size_t i = 0; i < items.size(); i++)
    {
        df::item *item = items[i];

        if (item->flags.whole & bad_flags.whole)
            continue;

        df::item_type itype = item->getType();
        int16_t isubtype = item->getSubtype();
        int16_t imattype = item->getActualMaterial();
        int32_t imatindex = item->getActualMaterialIndex();

        bool is_invalid = false;

        // Special handling
        switch (itype) {
        case item_type::BUCKET:
            if (dry_buckets && !item->flags.bits.in_job)
                dryBucket(item);
            break;

        case item_type::THREAD:
            if (item->getTotalDimension() < 15000)
                is_invalid = true;
            break;

        case item_type::CLOTH:
            if (item->getTotalDimension() < 10000)
                is_invalid = true;
            break;
        }

        if (item->flags.bits.melt && !item->flags.bits.owned && !itemBusy(item))
            meltable_count++;

        // Match to constraints
        TMaterialCache::key_type matkey(imattype, imatindex);

        for (size_t i = 0; i < constraints.size(); i++)
        {
            ItemConstraint *cv = constraints[i];
            if (cv->item.type != itype ||
                (cv->item.subtype != -1 && cv->item.subtype != isubtype))
                continue;

            TMaterialCache::iterator it = cv->material_cache.find(matkey);

            bool ok = true;
            if (it != cv->material_cache.end())
                ok = it->second;
            else
            {
                MaterialInfo mat(imattype, imatindex);
                ok = mat.matches(cv->material) &&
                     (cv->mat_mask.whole == 0 || mat.matches(cv->mat_mask));
                cv->material_cache[matkey] = ok;
            }

            if (!ok)
                continue;

            if (is_invalid ||
                item->flags.bits.owned ||
                item->flags.bits.in_chest ||
                item->isAssignedToStockpile() ||
                itemInRealJob(item) ||
                itemBusy(item))
            {
                cv->item_inuse++;
            }
            else
            {
                cv->item_count++;
                cv->item_amount += item->getStackSize();
            }
        }
    }

    for (size_t i = 0; i < constraints.size(); i++)
        constraints[i]->computeRequest();
}

/******************************
 *   ITEM COUNT CONSTRAINT    *
 ******************************/

static std::string shortJobDescription(df::job *job);

static void setJobResumed(Core *c, ProtectedJob *pj, bool goal)
{
    bool current = pj->isResumed();

    pj->set_resumed(goal);

    if (goal != current)
    {
        c->con.print("%s %s%s\n",
                     (goal ? "Resuming" : "Suspending"),
                     shortJobDescription(pj->actual_job).c_str(),
                     (!goal || pj->isActuallyResumed() ? "" : " (delayed)"));
    }
}

static void update_jobs_by_constraints(Core *c)
{
    for (TKnownJobs::const_iterator it = known_jobs.begin(); it != known_jobs.end(); ++it)
    {
        ProtectedJob *pj = it->second;
        if (!pj->isLive())
            continue;

        if (pj->actual_job->job_type == job_type::MeltMetalObject &&
            isOptionEnabled(CF_AUTOMELT))
        {
            setJobResumed(c, pj, meltable_count > 0);
            continue;
        }

        if (pj->constraints.empty())
            continue;

        int resume_weight = -1;
        int suspend_weight = -1;

        for (size_t i = 0; i < pj->constraints.size(); i++)
        {
            if (pj->constraints[i]->request_resume)
                resume_weight = std::max(resume_weight, pj->constraints[i]->weight);
            if (pj->constraints[i]->request_suspend)
                suspend_weight = std::max(suspend_weight, pj->constraints[i]->weight);
        }

        bool goal = pj->isResumed();

        if (resume_weight >= 0 && resume_weight >= suspend_weight)
            goal = true;
        else if (suspend_weight >= 0 && suspend_weight >= resume_weight)
            goal = false;

        setJobResumed(c, pj, goal);
    }

    for (size_t i = 0; i < constraints.size(); i++)
    {
        ItemConstraint *ct = constraints[i];

        bool is_running = false;
        for (size_t j = 0; j < ct->jobs.size(); j++)
            if (!!(is_running = ct->jobs[j]->isResumed()))
                break;

        std::string info = ct->item.toString();

        if (ct->material.isValid())
            info = ct->material.toString() + " " + info;
        else if (ct->mat_mask.whole)
            info = bitfieldToString(ct->mat_mask) + " " + info;

        if (is_running != ct->is_active)
        {
            if (is_running && ct->request_resume)
                showAnnouncement("Resuming production: " + info, 2, false);
            else if (!is_running && !ct->request_resume)
                showAnnouncement("Stopping production: " + info, 3, false);
        }

        if (ct->request_resume && !is_running)
        {
            if (!ct->cant_resume_reported)
                showAnnouncement("Cannot produce: " + info, 6, true);
            ct->cant_resume_reported = true;
        }
        else
        {
            ct->cant_resume_reported = false;
        }
    }
}

static void process_constraints(Core *c)
{
    if (constraints.empty() &&
        !isOptionEnabled(CF_DRYBUCKETS | CF_AUTOMELT))
        return;

    map_job_constraints(c);
    map_job_items(c);
    update_jobs_by_constraints(c);
}

/******************************
 *  PRINTING AND THE COMMAND  *
 ******************************/

static std::string shortJobDescription(df::job *job)
{
    std::string rv = stl_sprintf("job %d: ", job->id);

    if (job->job_type != job_type::CustomReaction)
        rv += ENUM_KEY_STR(job_type, job->job_type);
    else
        rv += job->reaction_name;

    MaterialInfo mat;
    df::dfhack_material_category mat_mask;
    guess_job_material(job, mat, mat_mask);

    if (mat.isValid())
        rv += " [" + mat.toString() + "]";
    else if (mat_mask.whole)
        rv += " [" + bitfieldToString(mat_mask) + "]";

    return rv;
}

static void print_constraint(Core *c, ItemConstraint *cv, bool no_job = false, std::string prefix = "")
{
    Console::color_value color;
    if (cv->request_resume)
        color = Console::COLOR_GREEN;
    else if (cv->request_suspend)
        color = Console::COLOR_CYAN;
    else
        color = Console::COLOR_DARKGREY;

    c->con.color(color);
    c->con << prefix << "Constraint " << flush;
    c->con.color(Console::COLOR_GREY);
    c->con << cv->config.val() << " " << flush;
    c->con.color(color);
    c->con << (cv->goalByCount() ? "count " : "amount ")
           << cv->goalCount() << " (gap " << cv->goalGap() << ")" << endl;
    c->con.reset_color();

    if (cv->item_count || cv->item_inuse)
        c->con << prefix << "  items: amount " << cv->item_amount << "; "
                         << cv->item_count << " stacks available, "
                         << cv->item_inuse << " in use." << endl;

    if (no_job) return;

    if (cv->jobs.empty())
        c->con.printerr("  (no jobs)\n");

    std::vector<ProtectedJob*> unique_jobs;
    std::vector<int> unique_counts;

    for (size_t i = 0; i < cv->jobs.size(); i++)
    {
        ProtectedJob *pj = cv->jobs[i];
        for (size_t j = 0; j < unique_jobs.size(); j++)
        {
            if (unique_jobs[j]->building_id == pj->building_id &&
                *unique_jobs[j]->actual_job == *pj->actual_job)
            {
                unique_counts[j]++;
                goto next_job;
            }
        }

        unique_jobs.push_back(pj);
        unique_counts.push_back(1);
    next_job:;
    }

    for (size_t i = 0; i < unique_jobs.size(); i++)
    {
        ProtectedJob *pj = unique_jobs[i];
        df::job *job = pj->actual_job;

        std::string start = prefix + "  " + shortJobDescription(job);

        if (!pj->isActuallyResumed())
        {
            if (pj->want_resumed)
            {
                c->con.color(Console::COLOR_YELLOW);
                c->con << start << " (delayed)" << endl;
            }
            else
            {
                c->con.color(Console::COLOR_BLUE);
                c->con << start << " (suspended)" << endl;
            }
        }
        else
        {
            c->con.color(Console::COLOR_GREEN);
            c->con << start << endl;
        }

        c->con.reset_color();

        if (unique_counts[i] > 1)
            c->con << prefix << "    (" << unique_counts[i] << " copies)" << endl;
    }
}

static void print_job(Core *c, ProtectedJob *pj)
{
    if (!pj)
        return;

    df::job *job = pj->isLive() ? pj->actual_job : pj->job_copy;

    printJobDetails(c, job);

    if (job->job_type == job_type::MeltMetalObject &&
        isOptionEnabled(CF_AUTOMELT))
    {
        if (meltable_count <= 0)
            c->con.color(Console::COLOR_CYAN);
        else if (pj->want_resumed && !pj->isActuallyResumed())
            c->con.color(Console::COLOR_YELLOW);
        else
            c->con.color(Console::COLOR_GREEN);
        c->con << "  Meltable: " << meltable_count << " objects." << endl;
        c->con.reset_color();
    }

    for (size_t i = 0; i < pj->constraints.size(); i++)
        print_constraint(c, pj->constraints[i], true, "  ");
}

static command_result workflow_cmd(Core *c, vector <string> & parameters)
{
    CoreSuspender suspend(c);

    if (!c->isWorldLoaded()) {
        c->con.printerr("World is not loaded: please load a game first.\n");
        return CR_FAILURE;
    }

    if (enabled) {
        check_lost_jobs(c, 0);
        recover_jobs(c);
        update_job_data(c);
        map_job_constraints(c);
        map_job_items(c);
    }

    df::building *workshop = NULL;
    df::job *job = NULL;

    if (dwarfmode_hotkey(c, c->getTopViewscreen()) &&
        ui->main.mode == ui_sidebar_mode::QueryBuilding)
    {
        workshop = world->selected_building;
        job = getSelectedWorkshopJob(c, true);
    }

    std::string cmd = parameters.empty() ? "list" : parameters[0];

    if (cmd == "enable" || cmd == "disable")
    {
        bool enable = (cmd == "enable");
        if (enable && !enabled)
        {
            enable_plugin(c);
        }
        else if (!enable && parameters.size() == 1)
        {
            if (enabled)
            {
                enabled = false;
                setOptionEnabled(CF_ENABLED, false);
                stop_protect(c);
            }

            c->con << "The plugin is disabled." << endl;
            return CR_OK;
        }

        for (size_t i = 1; i < parameters.size(); i++)
        {
            if (parameters[i] == "drybuckets")
                setOptionEnabled(CF_DRYBUCKETS, enable);
            else if (parameters[i] == "auto-melt")
                setOptionEnabled(CF_AUTOMELT, enable);
            else
                return CR_WRONG_USAGE;
        }

        if (enabled)
            c->con << "The plugin is enabled." << endl;
        else
            c->con << "The plugin is disabled." << endl;

        if (isOptionEnabled(CF_DRYBUCKETS))
            c->con << "Option drybuckets is enabled." << endl;
        if (isOptionEnabled(CF_AUTOMELT))
            c->con << "Option auto-melt is enabled." << endl;

        return CR_OK;
    }
    else if (cmd == "count" || cmd == "amount")
    {
        if (!enabled)
            enable_plugin(c);
    }

    if (!enabled)
        c->con << "Note: the plugin is not enabled." << endl;

    if (cmd == "jobs")
    {
        if (workshop)
        {
            for (size_t i = 0; i < workshop->jobs.size(); i++)
                print_job(c, get_known(workshop->jobs[i]->id));
        }
        else
        {
            for (TKnownJobs::iterator it = known_jobs.begin(); it != known_jobs.end(); ++it)
                if (it->second->isLive())
                    print_job(c, it->second);
        }

        bool pending = false;

        for (size_t i = 0; i < pending_recover.size(); i++)
        {
            if (!workshop || pending_recover[i]->holder == workshop)
            {
                if (!pending)
                {
                    c->con.print("\nPending recovery:\n");
                    pending = true;
                }

                printJobDetails(c, pending_recover[i]->job_copy);
            }
        }

        return CR_OK;
    }
    else if (cmd == "list")
    {
        for (size_t i = 0; i < constraints.size(); i++)
            print_constraint(c, constraints[i]);

        return CR_OK;
    }
    else if (cmd == "count" || cmd == "amount")
    {
        if (parameters.size() < 3)
            return CR_WRONG_USAGE;

        int limit = atoi(parameters[2].c_str());
        if (limit <= 0) {
            c->con.printerr("Invalid limit value.\n");
            return CR_FAILURE;
        }

        ItemConstraint *icv = get_constraint(c, parameters[1]);
        if (!icv)
            return CR_FAILURE;

        icv->setGoalByCount(cmd == "count");
        icv->setGoalCount(limit);
        if (parameters.size() >= 4)
            icv->setGoalGap(atoi(parameters[3].c_str()));
        else
            icv->setGoalGap(-1);

        process_constraints(c);
        print_constraint(c, icv);
        return CR_OK;
    }
    else if (cmd == "unlimit")
    {
        if (parameters.size() != 2)
            return CR_WRONG_USAGE;

        for (size_t i = 0; i < constraints.size(); i++)
        {
            if (constraints[i]->config.val() != parameters[1])
                continue;

            delete_constraint(c, constraints[i]);
            return CR_OK;
        }

        c->con.printerr("Constraint not found: %s\n", parameters[1].c_str());
        return CR_FAILURE;
    }
    else
        return CR_WRONG_USAGE;
}
