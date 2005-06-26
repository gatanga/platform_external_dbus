/* -*- mode: C; c-file-style: "gnu" -*- */
#include <dbus/dbus-glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test-service-glib-bindings.h"
#include <glib/dbus-gidl.h>
#include <glib/dbus-gparser.h>
#include <glib-object.h>
#include "my-object-marshal.h"

static GMainLoop *loop = NULL;
static int n_times_foo_received = 0;
static int n_times_frobnicate_received = 0;
static int n_times_sig0_received = 0;
static int n_times_sig1_received = 0;
static guint exit_timeout = 0;

static gboolean
timed_exit (gpointer loop)
{
  g_main_loop_quit (loop);
  return TRUE;
}

static void
foo_signal_handler (DBusGProxy  *proxy,
                    double       d,
                    void        *user_data)
{
  n_times_foo_received += 1;

  g_main_loop_quit (loop);
  g_source_remove (exit_timeout);
}

static void
frobnicate_signal_handler (DBusGProxy  *proxy,
			   int          val,
			   void        *user_data)
{
  n_times_frobnicate_received += 1;

  g_assert (val == 42);

  g_main_loop_quit (loop);
  g_source_remove (exit_timeout);
}

static void
sig0_signal_handler (DBusGProxy  *proxy,
		     const char  *str0,
		     int          val,
		     const char  *str1,
		     void        *user_data)
{
  n_times_sig0_received += 1;

  g_assert (!strcmp (str0, "foo"));

  g_assert (val == 22);

  g_assert (!strcmp (str1, "moo"));

  g_main_loop_quit (loop);
  g_source_remove (exit_timeout);
}

static void
sig1_signal_handler (DBusGProxy  *proxy,
		     const char  *str0,
		     GValue      *value,
		     void        *user_data)
{
  n_times_sig1_received += 1;

  g_assert (!strcmp (str0, "baz"));

  g_assert (G_VALUE_HOLDS_STRING (value));

  g_assert (!strcmp (g_value_get_string (value), "bar"));

  g_main_loop_quit (loop);
  g_source_remove (exit_timeout);
}

static void lose (const char *fmt, ...) G_GNUC_NORETURN G_GNUC_PRINTF (1, 2);
static void lose_gerror (const char *prefix, GError *error) G_GNUC_NORETURN;

static void
lose (const char *str, ...)
{
  va_list args;

  va_start (args, str);

  vfprintf (stderr, str, args);
  fputc ('\n', stderr);

  va_end (args);

  exit (1);
}

static void
lose_gerror (const char *prefix, GError *error) 
{
  lose ("%s: %s", prefix, error->message);
}

int
main (int argc, char **argv)
{
  DBusGConnection *connection;
  GError *error;
  DBusGProxy *driver;
  DBusGProxy *proxy;
  DBusGPendingCall *call;
  char **name_list;
  guint name_list_len;
  guint i;
  guint32 result;
  const char *v_STRING;
  char *v_STRING_2;
  guint32 v_UINT32;
  guint32 v_UINT32_2;
  double v_DOUBLE;
  double v_DOUBLE_2;
    
  g_type_init ();

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING);
  
  loop = g_main_loop_new (NULL, FALSE);

  error = NULL;
  connection = dbus_g_bus_get (DBUS_BUS_SESSION,
                               &error);
  if (connection == NULL)
    lose_gerror ("Failed to open connection to bus", error);

  /* should always get the same one */
  g_assert (connection == dbus_g_bus_get (DBUS_BUS_SESSION, NULL));
  g_assert (connection == dbus_g_bus_get (DBUS_BUS_SESSION, NULL));
  g_assert (connection == dbus_g_bus_get (DBUS_BUS_SESSION, NULL));
  
  /* Create a proxy object for the "bus driver" */
  
  driver = dbus_g_proxy_new_for_name (connection,
                                      DBUS_SERVICE_DBUS,
                                      DBUS_PATH_DBUS,
                                      DBUS_INTERFACE_DBUS);

  /* Call ListNames method */
  
  call = dbus_g_proxy_begin_call (driver, "ListNames", G_TYPE_INVALID);

  error = NULL;
  if (!dbus_g_proxy_end_call (driver, call, &error,
                              G_TYPE_STRV, &name_list,
                              G_TYPE_INVALID))
    lose_gerror ("Failed to complete ListNames call", error);

  g_print ("Names on the message bus:\n");
  i = 0;
  name_list_len = g_strv_length (name_list);
  while (i < name_list_len)
    {
      g_assert (name_list[i] != NULL);
      g_print ("  %s\n", name_list[i]);
      ++i;
    }
  g_assert (name_list[i] == NULL);

  g_strfreev (name_list);

  /* Test handling of unknown method */
  call = dbus_g_proxy_begin_call (driver, "ThisMethodDoesNotExist",
                                  G_TYPE_STRING,
                                  "blah blah blah blah blah",
                                  G_TYPE_INT,
                                  10,
                                  G_TYPE_INVALID);

  error = NULL;
  if (dbus_g_proxy_end_call (driver, call, &error,
			     G_TYPE_INVALID))
    lose ("Calling nonexistent method succeeded!");

  g_print ("Got EXPECTED error from calling unknown method: %s\n", error->message);
  g_error_free (error);
  
  /* Activate a service */
  call = dbus_g_proxy_begin_call (driver, "StartServiceByName",
                                  G_TYPE_STRING,
                                  "org.freedesktop.DBus.TestSuiteEchoService",
                                  G_TYPE_UINT,
                                  0,
                                  G_TYPE_INVALID);

  error = NULL;
  if (!dbus_g_proxy_end_call (driver, call, &error,
                              G_TYPE_UINT, &result,
                              G_TYPE_INVALID))
    lose_gerror ("Failed to complete Activate call", error);

  g_print ("Starting echo service result = 0x%x\n", result);

  /* Activate a service again */
  call = dbus_g_proxy_begin_call (driver, "StartServiceByName",
                                  G_TYPE_STRING,
                                  "org.freedesktop.DBus.TestSuiteEchoService",
                                  G_TYPE_UINT,
                                  0,
                                  DBUS_TYPE_INVALID);

  error = NULL;
  if (!dbus_g_proxy_end_call (driver, call, &error,
			      G_TYPE_UINT, &result,
			      G_TYPE_INVALID))
    lose_gerror ("Failed to complete Activate call", error);

  g_print ("Duplicate start of echo service = 0x%x\n", result);

  /* Talk to the new service */
  
  proxy = dbus_g_proxy_new_for_name_owner (connection,
                                           "org.freedesktop.DBus.TestSuiteEchoService",
                                           "/org/freedesktop/TestSuite",
                                           "org.freedesktop.TestSuite",
                                           &error);
  
  if (proxy == NULL)
    lose_gerror ("Failed to create proxy for name owner", error);

  call = dbus_g_proxy_begin_call (proxy, "Echo",
                                  G_TYPE_STRING,
                                  "my string hello",
                                  G_TYPE_INVALID);

  error = NULL;
  if (!dbus_g_proxy_end_call (proxy, call, &error,
                              G_TYPE_STRING, &v_STRING_2,
                              G_TYPE_INVALID))
    lose_gerror ("Failed to complete Echo call", error);

  g_print ("String echoed = \"%s\"\n", v_STRING_2);
  g_free (v_STRING_2);

  /* Test oneway call and signal handling */

  dbus_g_proxy_add_signal (proxy, "Foo", G_TYPE_DOUBLE, G_TYPE_INVALID);
  
  dbus_g_proxy_connect_signal (proxy, "Foo",
                               G_CALLBACK (foo_signal_handler),
                               NULL, NULL);
  
  dbus_g_proxy_call_no_reply (proxy, "EmitFoo",
                              G_TYPE_INVALID);
  
  dbus_g_connection_flush (connection);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);

  if (n_times_foo_received != 1)
    lose ("Foo signal received %d times, should have been 1", n_times_foo_received);
  
  /* Activate test servie */ 
  g_print ("Activating TestSuiteGLibService\n");
  error = NULL;
  if (!dbus_g_proxy_call (driver, "StartServiceByName", &error,
			  G_TYPE_STRING,
			  "org.freedesktop.DBus.TestSuiteGLibService",
			  G_TYPE_UINT,
			  0,
			  G_TYPE_INVALID,
			  G_TYPE_UINT, &result,
			  G_TYPE_INVALID)) {
    lose_gerror ("Failed to complete Activate call", error);
  }

  g_print ("TestSuiteGLibService activated\n");

  if (getenv ("DBUS_GLIB_TEST_SLEEP_AFTER_ACTIVATION"))
    g_usleep (8 * G_USEC_PER_SEC);

  g_object_unref (G_OBJECT (proxy));

  proxy = dbus_g_proxy_new_for_name_owner (connection,
                                           "org.freedesktop.DBus.TestSuiteGLibService",
                                           "/org/freedesktop/DBus/Tests/MyTestObject",
                                           "org.freedesktop.DBus.Tests.MyObject",
                                           &error);
  
  if (proxy == NULL)
    lose_gerror ("Failed to create proxy for name owner", error);

  g_print ("Beginning method calls\n");

  call = dbus_g_proxy_begin_call (proxy, "DoNothing",
                                  G_TYPE_INVALID);
  error = NULL;
  if (!dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID))
    lose_gerror ("Failed to complete DoNothing call", error);

  error = NULL;
  if (!dbus_g_proxy_call (proxy, "Increment", &error,
			  G_TYPE_UINT, 42,
			  G_TYPE_INVALID,
			  G_TYPE_UINT, &v_UINT32_2,
			  G_TYPE_INVALID))
    lose_gerror ("Failed to complete Increment call", error);

  if (v_UINT32_2 != 43)
    lose ("Increment call returned %d, should be 43", v_UINT32_2);

  call = dbus_g_proxy_begin_call (proxy, "ThrowError", G_TYPE_INVALID);
  error = NULL;
  if (dbus_g_proxy_end_call (proxy, call, &error, G_TYPE_INVALID) != FALSE)
    lose ("ThrowError call unexpectedly succeeded!");
  if (!dbus_g_error_has_name (error, "org.freedesktop.DBus.Tests.MyObject.Foo"))
    lose ("ThrowError call returned unexpected error %s", dbus_g_error_get_name (error));

  g_print ("ThrowError failed (as expected) returned error: %s\n", error->message);
  g_clear_error (&error);

  call = dbus_g_proxy_begin_call (proxy, "Uppercase",
				  G_TYPE_STRING, "foobar",
				  G_TYPE_INVALID);
  error = NULL;
  if (!dbus_g_proxy_end_call (proxy, call, &error,
			      G_TYPE_STRING, &v_STRING_2,
			      G_TYPE_INVALID))
    lose_gerror ("Failed to complete Uppercase call", error);
  if (strcmp ("FOOBAR", v_STRING_2) != 0)
    lose ("Uppercase call returned unexpected string %s", v_STRING_2);
  g_free (v_STRING_2);

  v_STRING = "bazwhee";
  v_UINT32 = 26;
  v_DOUBLE = G_PI;
  call = dbus_g_proxy_begin_call (proxy, "ManyArgs",
				  G_TYPE_UINT, 26,
				  G_TYPE_STRING, "bazwhee",
				  G_TYPE_DOUBLE, G_PI,
				  G_TYPE_INVALID);
  error = NULL;
  if (!dbus_g_proxy_end_call (proxy, call, &error,
			      G_TYPE_DOUBLE, &v_DOUBLE_2,
			      G_TYPE_STRING, &v_STRING_2,
			      G_TYPE_INVALID))
    lose_gerror ("Failed to complete ManyArgs call", error);
  if (v_DOUBLE_2 < 55 || v_DOUBLE_2 > 56)
    lose ("ManyArgs call returned unexpected double value %f", v_DOUBLE_2);
  if (strcmp ("BAZWHEE", v_STRING_2) != 0)
    lose ("ManyArgs call returned unexpected string %s", v_STRING_2);
  g_free (v_STRING_2);

  if (!org_freedesktop_DBus_Tests_MyObject_do_nothing (proxy, &error))
    lose_gerror ("Failed to complete (wrapped) DoNothing call", error);

  if (!org_freedesktop_DBus_Tests_MyObject_increment (proxy, 42, &v_UINT32_2, &error))
    lose_gerror ("Failed to complete (wrapped) Increment call", error);

  if (v_UINT32_2 != 43)
    lose ("(wrapped) increment call returned %d, should be 43", v_UINT32_2);

  if (org_freedesktop_DBus_Tests_MyObject_throw_error (proxy, &error) != FALSE)
    lose ("(wrapped) ThrowError call unexpectedly succeeded!");

  g_print ("(wrapped) ThrowError failed (as expected) returned error: %s\n", error->message);
  g_clear_error (&error);

  if (!org_freedesktop_DBus_Tests_MyObject_uppercase (proxy, "foobar", &v_STRING_2, &error)) 
    lose_gerror ("Failed to complete (wrapped) Uppercase call", error);
  if (strcmp ("FOOBAR", v_STRING_2) != 0)
    lose ("(wrapped) Uppercase call returned unexpected string %s", v_STRING_2);
  g_free (v_STRING_2);

  if (!org_freedesktop_DBus_Tests_MyObject_many_args (proxy, 26, "bazwhee", G_PI,
						      &v_DOUBLE_2, &v_STRING_2, &error))
    lose_gerror ("Failed to complete (wrapped) ManyArgs call", error);

  if (v_DOUBLE_2 < 55 || v_DOUBLE_2 > 56)
    
    lose ("(wrapped) ManyArgs call returned unexpected double value %f", v_DOUBLE_2);

  if (strcmp ("BAZWHEE", v_STRING_2) != 0)
    lose ("(wrapped) ManyArgs call returned unexpected string %s", v_STRING_2);
  g_free (v_STRING_2);

  {
    guint32 arg0;
    char *arg1;
    gint32 arg2;
    guint32 arg3;
    guint32 arg4;
    char *arg5;
    
    if (!org_freedesktop_DBus_Tests_MyObject_many_return (proxy, &arg0, &arg1, &arg2, &arg3, &arg4, &arg5, &error))
      lose_gerror ("Failed to complete (wrapped) ManyReturn call", error);

    if (arg0 != 42)
      lose ("(wrapped) ManyReturn call returned unexpected guint32 value %u", arg0);

    if (strcmp ("42", arg1) != 0)
      lose ("(wrapped) ManyReturn call returned unexpected string %s", arg1);
    g_free (arg1);

    if (arg2 != -67)
      lose ("(wrapped) ManyReturn call returned unexpected gint32 value %u", arg2);

    if (arg3 != 2)
      lose ("(wrapped) ManyReturn call returned unexpected guint32 value %u", arg3);

    if (arg4 != 26)
      lose ("(wrapped) ManyReturn call returned unexpected guint32 value %u", arg4);

    if (strcmp ("hello world", arg5))
      lose ("(wrapped) ManyReturn call returned unexpected string %s", arg5);
    g_free (arg5);
  }

  {
    GValue value = {0, };

    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, "foo");

    if (!org_freedesktop_DBus_Tests_MyObject_stringify (proxy,
							&value,
							&v_STRING_2,
							&error))
      lose_gerror ("Failed to complete (wrapped) stringify call", error);
    if (strcmp ("foo", v_STRING_2) != 0)
      lose ("(wrapped) stringify call returned unexpected string %s", v_STRING_2);
    g_free (v_STRING_2);

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 42);

    if (!org_freedesktop_DBus_Tests_MyObject_stringify (proxy,
							&value,
							&v_STRING_2,
							&error))
      lose_gerror ("Failed to complete (wrapped) stringify call 2", error);
    if (strcmp ("42", v_STRING_2) != 0)
      lose ("(wrapped) stringify call 2 returned unexpected string %s", v_STRING_2);
    g_value_unset (&value);
    g_free (v_STRING_2);

    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, 88);
    if (!org_freedesktop_DBus_Tests_MyObject_stringify (proxy,
							&value,
							NULL,
							&error))
      lose_gerror ("Failed to complete (wrapped) stringify call 3", error);
    g_value_unset (&value);

    if (!org_freedesktop_DBus_Tests_MyObject_unstringify (proxy,
							  "foo",
							  &value,
							  &error))
      lose_gerror ("Failed to complete (wrapped) unstringify call", error);
    if (!G_VALUE_HOLDS_STRING (&value))
      lose ("(wrapped) unstringify call returned unexpected value type %d", (int) G_VALUE_TYPE (&value));
    if (strcmp (g_value_get_string (&value), "foo"))
      lose ("(wrapped) unstringify call returned unexpected string %s",
	    g_value_get_string (&value));
	
    g_value_unset (&value);

    if (!org_freedesktop_DBus_Tests_MyObject_unstringify (proxy,
							  "10",
							  &value,
							  &error))
      lose_gerror ("Failed to complete (wrapped) unstringify call", error);
    if (!G_VALUE_HOLDS_INT (&value))
      lose ("(wrapped) unstringify call returned unexpected value type %d", (int) G_VALUE_TYPE (&value));
    if (g_value_get_int (&value) != 10)
      lose ("(wrapped) unstringify call returned unexpected integer %d",
	    g_value_get_int (&value));

    g_value_unset (&value);
  }

  {
    GArray *array;
    guint32 val;
    guint32 arraylen;

    array = g_array_new (FALSE, TRUE, sizeof (guint32));
    val = 42;
    g_array_append_val (array, val);
    val = 69;
    g_array_append_val (array, val);
    val = 88;
    g_array_append_val (array, val);
    val = 26;
    g_array_append_val (array, val);
    val = 2;
    g_array_append_val (array, val);

    arraylen = 0;
    if (!org_freedesktop_DBus_Tests_MyObject_recursive1 (proxy, array,
							 &arraylen, &error))
      lose_gerror ("Failed to complete (wrapped) recursive1 call", error);
    if (arraylen != 5)
      lose ("(wrapped) recursive1 call returned invalid length %u", arraylen);
  }

  {
    GArray *array = NULL;
    guint32 *arrayvals;
    
    if (!org_freedesktop_DBus_Tests_MyObject_recursive2 (proxy, 2, &array, &error))
      lose_gerror ("Failed to complete (wrapped) Recursive2 call", error);

    if (array == NULL)
      lose ("(wrapped) Recursive2 call returned NULL");
    if (array->len != 5)
      lose ("(wrapped) Recursive2 call returned unexpected array length %u", array->len);

    arrayvals = (guint32*) array->data;
    if (arrayvals[0] != 42)
      lose ("(wrapped) Recursive2 call returned unexpected value %d in position 0", arrayvals[0]);
    if (arrayvals[1] != 26)
      lose ("(wrapped) Recursive2 call returned unexpected value %d in position 1", arrayvals[1]);
    if (arrayvals[4] != 2)
      lose ("(wrapped) Recursive2 call returned unexpected value %d in position 4", arrayvals[4]);

    g_array_free (array, TRUE);
  }

  {
    char **strs;
    char **strs_ret;

    strs = g_new0 (char *, 4);
    strs[0] = "hello";
    strs[1] = "HellO";
    strs[2] = "HELLO";
    strs[3] = NULL;

    strs_ret = NULL;
    if (!org_freedesktop_DBus_Tests_MyObject_many_uppercase (proxy, strs, &strs_ret, &error)) 
      lose_gerror ("Failed to complete (wrapped) ManyUppercase call", error);
    g_assert (strs_ret != NULL);
    if (strcmp ("HELLO", strs_ret[0]) != 0)
      lose ("(wrapped) ManyUppercase call returned unexpected string %s", strs_ret[0]);
    if (strcmp ("HELLO", strs_ret[1]) != 0)
      lose ("(wrapped) ManyUppercase call returned unexpected string %s", strs_ret[1]);
    if (strcmp ("HELLO", strs_ret[2]) != 0)
      lose ("(wrapped) ManyUppercase call returned unexpected string %s", strs_ret[2]);

    g_strfreev (strs_ret);
  }

  {
    GHashTable *table;
    guint len;

    table = g_hash_table_new (g_str_hash, g_str_equal);
    g_hash_table_insert (table, "moooo", "b");
    g_hash_table_insert (table, "xxx", "cow!");

    len = 0;
    if (!org_freedesktop_DBus_Tests_MyObject_str_hash_len (proxy, table, &len, &error))
      lose_gerror ("(wrapped) StrHashLen call failed", error);
    if (len != 13) 
      lose ("(wrapped) StrHashLen returned unexpected length %u", len);
    g_hash_table_destroy (table);
  }

  {
    GHashTable *table;
    const char *val;

    if (!org_freedesktop_DBus_Tests_MyObject_get_hash (proxy, &table, &error))
      lose_gerror ("(wrapped) GetHash call failed", error);
    val = g_hash_table_lookup (table, "foo");
    if (val == NULL || strcmp ("bar", val))
      lose ("(wrapped) StrHashLen returned invalid value %s for key \"foo\"",
	    val ? val : "(null)");
    val = g_hash_table_lookup (table, "baz");
    if (val == NULL || strcmp ("whee", val))
      lose ("(wrapped) StrHashLen returned invalid value %s for key \"whee\"",
	    val ? val : "(null)");
    val = g_hash_table_lookup (table, "cow");
    if (val == NULL || strcmp ("crack", val))
      lose ("(wrapped) StrHashLen returned invalid value %s for key \"cow\"",
	    val ? val : "(null)");
    if (g_hash_table_size (table) != 3)
      lose ("(wrapped) StrHashLen returned unexpected hash size %u",
	    g_hash_table_size (table));

    g_hash_table_destroy (table);
  }

  {
    guint val;
    DBusGProxy *ret_proxy;

    if (!org_freedesktop_DBus_Tests_MyObject_objpath (proxy, proxy, &ret_proxy, &error))
      lose_gerror ("Failed to complete (wrapped) Objpath call", error);
    if (strcmp ("/org/freedesktop/DBus/Tests/MyTestObject2",
		dbus_g_proxy_get_path (ret_proxy)) != 0)
      lose ("(wrapped) objpath call returned unexpected proxy %s",
	    dbus_g_proxy_get_path (ret_proxy));

    val = 1;
    if (!org_freedesktop_DBus_Tests_MyObject_get_val (ret_proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetVal call", error);
    if (val != 0)
      lose ("(wrapped) GetVal returned invalid value %d", val);

    if (!org_freedesktop_DBus_Tests_MyObject_increment_val (ret_proxy, &error))
      lose_gerror ("Failed to complete (wrapped) IncrementVal call", error);

    if (!org_freedesktop_DBus_Tests_MyObject_increment_val (ret_proxy, &error))
      lose_gerror ("Failed to complete (wrapped) IncrementVal call", error);

    if (!org_freedesktop_DBus_Tests_MyObject_increment_val (ret_proxy, &error))
      lose_gerror ("Failed to complete (wrapped) IncrementVal call", error);

    if (!org_freedesktop_DBus_Tests_MyObject_get_val (ret_proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetVal call", error);
    if (val != 3)
      lose ("(wrapped) GetVal returned invalid value %d", val);

    if (!org_freedesktop_DBus_Tests_MyObject_get_val (proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetVal call", error);
    if (val != 0)
      lose ("(wrapped) GetVal returned invalid value %d", val);

    if (!org_freedesktop_DBus_Tests_MyObject_increment_val (proxy, &error))
      lose_gerror ("Failed to complete (wrapped) IncrementVal call", error);

    if (!org_freedesktop_DBus_Tests_MyObject_get_val (proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetVal call", error);
    if (val != 1)
      lose ("(wrapped) GetVal returned invalid value %d", val);

    if (!org_freedesktop_DBus_Tests_MyObject_get_val (ret_proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetVal call", error);
    if (val != 3)
      lose ("(wrapped) GetVal returned invalid value %d", val);

    g_object_unref (G_OBJECT (ret_proxy));

    ret_proxy = NULL;
    if (!org_freedesktop_DBus_Tests_MyObject_objpath (proxy, proxy, &ret_proxy, &error))
      lose_gerror ("Failed to complete (wrapped) Objpath call 2", error);
    if (strcmp ("/org/freedesktop/DBus/Tests/MyTestObject2",
		dbus_g_proxy_get_path (ret_proxy)) != 0)
      lose ("(wrapped) objpath call 2 returned unexpected proxy %s",
	    dbus_g_proxy_get_path (ret_proxy));
    {
      const char *iface = dbus_g_proxy_get_interface (ret_proxy);
      g_print ("returned proxy has interface \"%s\"\n",
	       iface ? iface : "(NULL)");
    }

    dbus_g_proxy_set_interface (ret_proxy, "org.freedesktop.DBus.Tests.FooObject");

    val = 0;
    if (!org_freedesktop_DBus_Tests_FooObject_get_value (ret_proxy, &val, &error))
      lose_gerror ("Failed to complete (wrapped) GetValue call", error);
    if (val != 3)
      lose ("(wrapped) GetValue returned invalid value %d", val);
  }

  /* Signal handling tests */
  
  dbus_g_proxy_add_signal (proxy, "Frobnicate", G_TYPE_INT, G_TYPE_INVALID);
  
  dbus_g_proxy_connect_signal (proxy, "Frobnicate",
                               G_CALLBACK (frobnicate_signal_handler),
                               NULL, NULL);
  
  if (!dbus_g_proxy_call (proxy, "EmitFrobnicate", &error,
			  G_TYPE_INVALID, G_TYPE_INVALID))
    lose_gerror ("Failed to complete EmitFrobnicate call", error);

  
  dbus_g_connection_flush (connection);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);

  if (n_times_frobnicate_received != 1)
    lose ("Frobnicate signal received %d times, should have been 1", n_times_frobnicate_received);

  if (!dbus_g_proxy_call (proxy, "EmitFrobnicate", &error,
			  G_TYPE_INVALID, G_TYPE_INVALID))
    lose_gerror ("Failed to complete EmitFrobnicate call", error);
  
  dbus_g_connection_flush (connection);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);

  if (n_times_frobnicate_received != 2)
    lose ("Frobnicate signal received %d times, should have been 2", n_times_frobnicate_received);

  g_object_unref (G_OBJECT (proxy));

  proxy = dbus_g_proxy_new_for_name_owner (connection,
                                           "org.freedesktop.DBus.TestSuiteGLibService",
                                           "/org/freedesktop/DBus/Tests/MyTestObject",
                                           "org.freedesktop.DBus.Tests.FooObject",
                                           &error);
  
  if (proxy == NULL)
    lose_gerror ("Failed to create proxy for name owner", error);

  dbus_g_object_register_marshaller (my_object_marshal_VOID__STRING_INT_STRING, 
				     G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INVALID);

  dbus_g_object_register_marshaller (my_object_marshal_VOID__STRING_BOXED, 
				     G_TYPE_NONE, G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);

  dbus_g_proxy_add_signal (proxy, "Sig0", G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_add_signal (proxy, "Sig1", G_TYPE_STRING, G_TYPE_VALUE);
  
  dbus_g_proxy_connect_signal (proxy, "Sig0",
                               G_CALLBACK (sig0_signal_handler),
                               NULL, NULL);
  dbus_g_proxy_connect_signal (proxy, "Sig1",
                               G_CALLBACK (sig1_signal_handler),
                               NULL, NULL);

  dbus_g_proxy_call_no_reply (proxy, "EmitSignals", G_TYPE_INVALID);

  dbus_g_connection_flush (connection);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);

  if (n_times_sig0_received != 1)
    lose ("Sig0 signal received %d times, should have been 1", n_times_sig0_received);
  if (n_times_sig1_received != 1)
    lose ("Sig1 signal received %d times, should have been 1", n_times_sig1_received);

  dbus_g_proxy_call_no_reply (proxy, "EmitSignals", G_TYPE_INVALID);
  dbus_g_proxy_call_no_reply (proxy, "EmitSignals", G_TYPE_INVALID);

  dbus_g_connection_flush (connection);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);
  exit_timeout = g_timeout_add (5000, timed_exit, loop);
  g_main_loop_run (loop);

  if (n_times_sig0_received != 3)
    lose ("Sig0 signal received %d times, should have been 3", n_times_sig0_received);
  if (n_times_sig1_received != 3)
    lose ("Sig1 signal received %d times, should have been 3", n_times_sig1_received);
  
  g_object_unref (G_OBJECT (proxy));

  proxy = dbus_g_proxy_new_for_name_owner (connection,
                                           "org.freedesktop.DBus.TestSuiteGLibService",
                                           "/org/freedesktop/DBus/Tests/MyTestObject",
                                           "org.freedesktop.DBus.Introspectable",
                                           &error);
  
  if (proxy == NULL)
    lose_gerror ("Failed to create proxy for name owner", error);

  call = dbus_g_proxy_begin_call (proxy, "Introspect",
				  G_TYPE_INVALID);
  error = NULL;
  if (!dbus_g_proxy_end_call (proxy, call, &error,
			      G_TYPE_STRING, &v_STRING_2,
			      G_TYPE_INVALID))
    lose_gerror ("Failed to complete Introspect call", error);

  /* Could just do strcmp(), but that seems more fragile */
  {
    NodeInfo *node;
    GSList *elt;
    gboolean found_introspectable;
    gboolean found_properties;
    gboolean found_myobject;
    gboolean found_fooobject;

    node = description_load_from_string (v_STRING_2, strlen (v_STRING_2), &error);
    if (!node)
      lose_gerror ("Failed to parse introspection data: %s", error);

    found_introspectable = FALSE;
    found_properties = FALSE;
    found_myobject = FALSE;
    found_fooobject = FALSE;
    for (elt = node_info_get_interfaces (node); elt ; elt = elt->next)
      {
	InterfaceInfo *iface = elt->data;

	if (!found_introspectable && strcmp (interface_info_get_name (iface), "org.freedesktop.DBus.Introspectable") == 0)
	  found_introspectable = TRUE;
	else if (!found_properties && strcmp (interface_info_get_name (iface), "org.freedesktop.DBus.Properties") == 0)
	  found_properties = TRUE;
	else if (!found_myobject && strcmp (interface_info_get_name (iface), "org.freedesktop.DBus.Tests.MyObject") == 0)
	  {
	    GSList *elt;
	    gboolean found_manyargs;
	    
	    found_myobject = TRUE;
	    
	    found_manyargs = FALSE;
	    for (elt = interface_info_get_methods (iface); elt; elt = elt->next)
	      {
		MethodInfo *method;

		method = elt->data;
		if (strcmp (method_info_get_name (method), "ManyArgs") == 0)
		  {
		    found_manyargs = TRUE;
		    break;
		  }
	      }
	    if (!found_manyargs)
	      lose ("Missing method org.freedesktop.DBus.Tests.MyObject.ManyArgs");
	  }
	else if (!found_fooobject && strcmp (interface_info_get_name (iface), "org.freedesktop.DBus.Tests.FooObject") == 0)
	  found_fooobject = TRUE;
	else
	  lose ("Unexpected or duplicate interface %s", interface_info_get_name (iface));
      }

    if (!(found_introspectable && found_myobject && found_properties))
      lose ("Missing interface"); 
  }
  g_free (v_STRING_2);
  
  g_object_unref (G_OBJECT (driver));

  g_print ("Successfully completed %s\n", argv[0]);
  
  return 0;
}
