<?php
/**
 * @file
 * Contains PasswordPolicy.
 */

/**
 * Defines a class used for managing Password Policies.
 */
class PasswordPolicy {
  public $name;
  public $config;
  public $items;
  public $cache;

  /**
   * PasswordPolicy::__construct
   *
   * @param Object
   *   Database object as returned by ctools_export_crud_load().
   */
  function __construct($policy = NULL) {
    if ($policy == NULL) {
      ctools_include('export');
      $policy = ctools_export_crud_new('password_policy');
    }

    $this->name = $policy->name;
    $this->config = isset($policy->config) ? unserialize($policy->config) : array();

    // Load all constraint objects.
    $this->load_plugins();
  }

  /**
   * Create the administation form for this policy.
   */
  function admin_form(&$form, &$form_state) {
    // Condition settings.
    $form['condition_title'] = array(
      '#type' => 'item',
      '#title' => t('Conditions'),
    );
    $form['condition'] = array(
      '#type' => 'vertical_tabs',
      '#attached' => array(
        'js' => array(drupal_get_path('module', 'password_policy') . '/password_policy.js'),
      ),
    );

    // Constraint settings.
    $form['constraint_title'] = array(
      '#type' => 'item',
      '#title' => t('Constraints'),
    );
    $form['constraint'] = array(
      '#type' => 'vertical_tabs',
      '#attached' => array(
        'js' => array(drupal_get_path('module', 'password_policy') . '/password_policy.js'),
      ),
    );

    // Item settings.
    $form['item_title'] = array(
      '#type' => 'item',
      '#title' => t('Other settings'),
    );
    $form['item'] = array(
      '#type' => 'container',
    );

    foreach ($this->items as $item) {
      $parent = $item->is_type('condition') ? 'condition' : ($item->is_type('constraint') ? 'constraint' : 'item');
      $form[$parent]  = $form[$parent] + $item->admin_form($form, $form_state);
    }

    return $form;
  }

  /**
   * Form submit callback for policy administration form.
   *
   * Collect configuration from all plugins into a single configuration variable.
   */
  function admin_form_submit($form, &$form_state) {
    foreach ($this->items as $item) {
      $item->admin_form_submit($form, $form_state);
    }
    $form_state['item']->config = serialize($this->config);
  }

  /**
   * Collect messages generated from all plugins.
   */
  function message() {
    $items = array();
    foreach ($this->active_constraints() as $constraint) {
      $items = array_merge($items, $constraint->message());
    }
    return $items;
  }

  /**
   * Check all constraints that a password complies with the policy.
   */
  function check($pass, $account) {
    $errors = array();
    foreach ($this->active_constraints() as $constraint) {
      if (!$constraint->check($pass, $account)) {
        $errors = array_merge($errors, $constraint->fail_message($pass, $account));
      }
    }
    return $errors;
  }

  /**
   * Return all constraints that are active.
   */
  function active_constraints() {
    $constraints = array();
    foreach ($this->get_items('constraint') as $constraint) {
      // Any constraint with non-default configuration is "active"
      if ($constraint->info['config'] != $constraint->config) {
        $constraints[] = $constraint;
      }
    }
    return $constraints;
  }

  /**
   * Return whether all active conditions match.
   */
  function match($account) {
    $does_match = FALSE;
    foreach ($this->get_items('condition') as $condition) {
      // Any condition with non-default configuration is "active"
      if ($condition->info['config'] != $condition->config) {
        $does_match = $does_match || $condition->match($account);
      }
    }
    return $does_match;
  }

  /**
   * Run cron for each of the items that is a cron item
   */
  function cron() {
    foreach ($this->get_items('cron') as $item) {
      if ($item->is_active()) {
        $item->cron();
      }
    }
  }

  /**
   * Run init for each of the items that is a cron item
   */
  function init($account) {
    foreach ($this->get_items('init') as $item) {
      $item->init($account);
    }
  }

  /**
   * Load all constraint and condition plugins and instantiate object
   * for each saving them in to the module.
   */
  function _load_plugin($info) {
    if (!isset($this->config[$info['name']])) {
      $this->config[$info['name']] = array();
    }
    $this->items[$info['name']] = PasswordPolicyConstraint::factory($info, $this);
  }

  /**
   * Get all items of a particular type.
   */
  function get_items($type) {
    $this->cache[$type] = isset($this->cache[$type]) ? $this->cache[$type] : array();
    if (empty($this->cache[$type])) {
      foreach ($this->items as $item) {
        if ($item->is_type($type)) {
          $this->cache[$type][] = $item;
        }
      }
    }
    return $this->cache[$type];
  }

  function load_plugins() {
    ctools_include('plugins');
    foreach (ctools_get_plugins('password_policy', 'constraint') as $info) {
      $this->_load_plugin($info);
    }
    foreach (ctools_get_plugins('password_policy', 'condition') as $info) {
      $this->_load_plugin($info);
    }
    foreach (ctools_get_plugins('password_policy', 'item') as $info) {
      $this->_load_plugin($info);
    }
  }

  /**
   * Retrieve all policies.
   */
  static function all_policies() {
    ctools_include('export');
    $policies = array();
    $infos = ctools_export_crud_load_all('password_policy');
    foreach ($infos as $name => $info) {
      $policies[$name] = new PasswordPolicy($info);
    }
    return $policies;
  }

  /**
   * Retrieve all enabled policies.
   */
  static function enabled_policies() {
    ctools_include('export');
    $policies = array();
    $infos = ctools_export_crud_load_all('password_policy');
    foreach ($infos as $name => $info) {
      if (empty($info->disabled)) {
        $policies[$name] = new PasswordPolicy($info);
      }
    }
    return $policies;
  }

  /**
   * Retrieve all policies that match the current account.
   */
  static function matched_policies($account) {
    $policies = array();
    foreach (PasswordPolicy::enabled_policies() as $policy) {
      if ($policy->match($account)) {
        $policies[] = $policy;
      }
    }
    return $policies;
  }
}
