/**
 * MojoShader; generate shader programs from bytecode of compiled
 *  Direct3D shaders.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#include <stdio.h>
#include <stdlib.h>
#include "mojoshader.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#if MOJOSHADER_DEBUG_MALLOC
static void *Malloc(int len)
{
    void *ptr = malloc(len + sizeof (int));
    int *store = (int *) ptr;
    printf("malloc() %d bytes (%p)\n", len, ptr);
    if (ptr == NULL) return NULL;
    *store = len;
    return (void *) (store + 1);
} // Malloc


static void Free(void *_ptr)
{
    int *ptr = (((int *) _ptr) - 1);
    int len = *ptr;
    printf("free() %d bytes (%p)\n", len, ptr);
    free(ptr);
} // Free
#else
#define Malloc NULL
#define Free NULL
#endif

static inline void do_indent(const unsigned int indent)
{
    unsigned int i;
    for (i = 0; i < indent; i++)
        printf("    ");
}

#define INDENT() do { if (indent) { do_indent(indent); } } while (0)


static const char *shader_type(const MOJOSHADER_shaderType s)
{
    switch (s)
    {
        case MOJOSHADER_TYPE_UNKNOWN: return "unknown";
        case MOJOSHADER_TYPE_PIXEL: return "pixel";
        case MOJOSHADER_TYPE_VERTEX: return "vertex";
        case MOJOSHADER_TYPE_GEOMETRY: return "geometry";
        default: return "(bogus value?)";
    } // switch

    return NULL;  // shouldn't hit this.
} // shader_type


static void print_shader(const char *fname, const MOJOSHADER_parseData *pd,
                         unsigned int indent)
{
    INDENT(); printf("PROFILE: %s\n", pd->profile);
    if (pd->error_count > 0)
    {
        int i;
        for (i = 0; i < pd->error_count; i++)
        {
            const MOJOSHADER_error *err = &pd->errors[i];
            INDENT();
            printf("%s:%d: ERROR: %s\n",
                    err->filename ? err->filename : fname,
                    err->error_position, err->error);
        } // for
    } // if
    else
    {
        INDENT(); printf("SHADER TYPE: %s\n", shader_type(pd->shader_type));
        INDENT(); printf("VERSION: %d.%d\n", pd->major_ver, pd->minor_ver);
        INDENT(); printf("INSTRUCTION COUNT: %d\n", (int) pd->instruction_count);

        INDENT(); printf("ATTRIBUTES:");
        if (pd->attribute_count == 0)
            printf(" (none.)\n");
        else
        {
            int i;
            printf("\n");
            for (i = 0; i < pd->attribute_count; i++)
            {
                static const char *usagenames[] = {
                    "position", "blendweight", "blendindices", "normal",
                    "psize", "texcoord", "tangent", "binormal", "tessfactor",
                    "positiont", "color", "fog", "depth", "sample"
                };
                const MOJOSHADER_attribute *a = &pd->attributes[i];
                char numstr[16] = { 0 };
                if (a->index != 0)
                    snprintf(numstr, sizeof (numstr), "%d", a->index);
                INDENT();
                printf("    * %s%s", usagenames[(int) a->usage], numstr);
                if (a->name != NULL)
                    printf(" (\"%s\")", a->name);
                printf("\n");
            } // for
        } // else

        INDENT(); printf("CONSTANTS:");
        if (pd->constant_count == 0)
            printf(" (none.)\n");
        else
        {
            int i;
            printf("\n");
            for (i = 0; i < pd->constant_count; i++)
            {
                static const char *typenames[] = { "float", "int", "bool" };
                const MOJOSHADER_constant *c = &pd->constants[i];
                INDENT(); 
                printf("    * %d: %s (", c->index, typenames[(int) c->type]);
                if (c->type == MOJOSHADER_UNIFORM_FLOAT)
                {
                    printf("%f %f %f %f", c->value.f[0], c->value.f[1],
                                          c->value.f[2], c->value.f[3]);
                } // if
                else if (c->type == MOJOSHADER_UNIFORM_INT)
                {
                    printf("%d %d %d %d", c->value.i[0], c->value.i[1],
                                          c->value.i[2], c->value.i[3]);
                } // else if
                else if (c->type == MOJOSHADER_UNIFORM_BOOL)
                {
                    printf("%s", c->value.b ? "true" : "false");
                } // else if
                else
                {
                    printf("???");
                } // else
                printf(")\n");
            } // for
        } // else

        INDENT(); printf("UNIFORMS:");
        if (pd->uniform_count == 0)
            printf(" (none.)\n");
        else
        {
            int i;
            printf("\n");
            for (i = 0; i < pd->uniform_count; i++)
            {
                static const char *typenames[] = { "float", "int", "bool" };
                const MOJOSHADER_uniform *u = &pd->uniforms[i];
                const char *arrayof = "";
                const char *constant = u->constant ? "const " : "";
                char arrayrange[64] = { '\0' };
                if (u->array_count > 0)
                {
                    arrayof = "array[";
                    snprintf(arrayrange, sizeof (arrayrange), "%d] ",
                             u->array_count);
                } // if

                INDENT();
                printf("    * %d: %s%s%s%s", u->index, constant, arrayof,
                        arrayrange, typenames[(int) u->type]);
                if (u->name != NULL)
                    printf(" (\"%s\")", u->name);
                printf("\n");
            } // for
        } // else

        INDENT(); printf("SAMPLERS:");
        if (pd->sampler_count == 0)
            printf(" (none.)\n");
        else
        {
            int i;
            printf("\n");
            for (i = 0; i < pd->sampler_count; i++)
            {
                static const char *typenames[] = { "2d", "cube", "volume" };
                const MOJOSHADER_sampler *s = &pd->samplers[i];
                INDENT();
                printf("    * %d: %s", s->index, typenames[(int) s->type]);
                if (s->name != NULL)
                    printf(" (\"%s\")", s->name);
                printf("\n");
            } // for
        } // else

        if (pd->output != NULL)
        {
            int i;
            INDENT();
            printf("OUTPUT:\n");
            indent++;
            INDENT();
            for (i = 0; i < pd->output_len; i++)
            {
                putchar((int) pd->output[i]);
                if (pd->output[i] == '\n')
                    INDENT();
            } // for
            printf("\n");
            indent--;
        } // if
    } // else
    printf("\n\n");
} // print_shader


static void print_effect(const char *fname, const MOJOSHADER_effect *effect,
                         const unsigned int indent)
{
    INDENT(); printf("PROFILE: %s\n", effect->profile);
    printf("\n");
    if (effect->error_count > 0)
    {
        int i;
        for (i = 0; i < effect->error_count; i++)
        {
            const MOJOSHADER_error *err = &effect->errors[i];
            INDENT();
            printf("%s:%d: ERROR: %s\n",
                    err->filename ? err->filename : fname,
                    err->error_position, err->error);
        } // for
    } // if
    else
    {
        int i, j, k;
        const MOJOSHADER_effectTechnique *technique = effect->techniques;
        const MOJOSHADER_effectTexture *texture = effect->textures;
        const MOJOSHADER_effectShader *shader = effect->shaders;

        for (i = 0; i < effect->technique_count; i++, technique++)
        {
            const MOJOSHADER_effectPass *pass = technique->passes;
            INDENT(); printf("TECHNIQUE #%d ('%s'):\n", i, technique->name);
            for (j = 0; j < technique->pass_count; j++, pass++)
            {
                const MOJOSHADER_effectState *state = pass->states;
                INDENT(); printf("    PASS #%d ('%s'):\n", j, pass->name);
                for (k = 0; k < pass->state_count; k++, state++)
                {
                    INDENT(); printf("        STATE 0x%X\n", state->type);
                } // for
            } // for
            printf("\n");
        } // for

        for (i = 0; i < effect->texture_count; i++, texture++)
        {
            INDENT();
            printf("TEXTURE #%d ('%s'): %u\n", i,
                    texture->name, texture->param);
        } // for

        printf("\n");

        for (i = 0; i < effect->shader_count; i++, shader++)
        {
            INDENT();
            printf("SHADER #%d: technique %u, pass %u\n", i,
                    shader->technique, shader->pass);
            print_shader(fname, shader->shader, indent + 1);
        } // for
    } // else
} // print_effect


static int do_parse(const char *fname, const unsigned char *buf,
                    const int len, const char *prof)
{
    int retval = 0;

    // magic for an effects file (!!! FIXME: I _think_).
    if ( (buf[0] == 0x01) && (buf[1] == 0x09) &&
         (buf[2] == 0xFF) && (buf[3] == 0xFE) )
    {
        const MOJOSHADER_effect *effect;
        effect = MOJOSHADER_parseEffect(prof, buf, len, 0, 0, Malloc, Free, 0);
        retval = (effect->error_count == 0);
        printf("EFFECT: %s\n", fname);
        print_effect(fname, effect, 1);
        MOJOSHADER_freeEffect(effect);
    } // if

    else  // do it as a regular compiled shader.
    {
        const MOJOSHADER_parseData *pd;
        pd = MOJOSHADER_parse(prof, buf, len, NULL, 0, Malloc, Free, NULL);
        retval = (pd->error_count == 0);
        printf("SHADER: %s\n", fname);
        print_shader(fname, pd, 1);
        MOJOSHADER_freeParseData(pd);
    } // else

    return retval;
} // do_parse


int main(int argc, char **argv)
{
    int retval = 0;

    printf("MojoShader testparse\n");
    printf("Compiled against changeset %s\n", MOJOSHADER_CHANGESET);
    printf("Linked against changeset %s\n", MOJOSHADER_changeset());
    printf("\n");

    if (argc <= 2)
        printf("\n\nUSAGE: %s <profile> [file1] ... [fileN]\n\n", argv[0]);
    else
    {
        const char *profile = argv[1];
        int i;

        for (i = 2; i < argc; i++)
        {
            FILE *io = fopen(argv[i], "rb");
            if (io == NULL)
                printf(" ... fopen('%s') failed.\n", argv[i]);
            else
            {
                unsigned char *buf = (unsigned char *) malloc(1000000);
                int rc = fread(buf, 1, 1000000, io);
                fclose(io);
                if (!do_parse(argv[i], buf, rc, profile))
                    retval = 1;
                free(buf);
            } // else
        } // for
    } // else

    return retval;
} // main

// end of testparse.c ...

