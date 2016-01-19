<?php
/**
 * @file
 * Administration.
 */

/**
 * Form callback: allows the user to complete a prior authorization.
 */
function commerce_sermepa_complete_form($form, &$form_state, $order, $transaction) {
  $form_state['order'] = $order;
  $form_state['transaction'] = $transaction;

  // Load and store the payment method instance for this transaction.
  $payment_method = commerce_payment_method_instance_load($transaction->instance_id);
  $form_state['payment_method'] = $payment_method;

  $balance = commerce_payment_order_balance($order);

  // Convert the balance to the transaction currency.
  if ($balance['currency_code'] != $transaction->currency_code) {
    $balance['amount'] = commerce_currency_convert($balance['amount'], $balance['currency_code'], $transaction->currency_code);
    $balance['currency_code'] = $transaction->currency_code;
  }

  if ($balance['amount'] > 0 && $balance['amount'] < $transaction->amount) {
    $amount = $balance['amount'];
  }
  else {
    $amount = $transaction->amount;
  }

  // Convert the price amount to a user friendly decimal value.
  $amount = number_format(commerce_currency_amount_to_decimal($amount, $transaction->currency_code), 2, '.', '');

  $description = implode('<br />', array(
    t('Authorization: @amount', array('@amount' => commerce_currency_format($transaction->amount, $transaction->currency_code))),
    t('Order balance: @balance', array('@balance' => commerce_currency_format($balance['amount'], $balance['currency_code']))),
  ));

  $form['amount'] = array(
    '#type' => 'item',
    '#title' => t('Capture amount'),
    '#description' => $description,
    '#value' => $amount,
    '#suffix' => check_plain($transaction->currency_code),
  );
  $form_state['amount'] = $amount;

  $form = confirm_form($form,
    t('Do you want complete this transaction?'),
    'admin/commerce/orders/' . $order->order_id . '/payment',
    '',
    t('Capture'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Authorization complete submit.
 */
function commerce_sermepa_complete_form_submit($form, &$form_state) {
  $transaction = $form_state['transaction'];

  $order = $form_state['order'];
  $order_wrapper = entity_metadata_wrapper('commerce_order', $order);
  $order_total = $order_wrapper->commerce_order_total->value();
  $order_total['amount'] = commerce_currency_convert($order_total['amount'], $order_total['currency_code'], $transaction->currency_code);

  // Convert the amout to the commerce format.
  $amount = commerce_currency_decimal_to_amount($form_state['amount'], $transaction->currency_code);

  // Create a sermepa instance.
  if (!$gateway = commerce_sermepa_library_initialize()) {
    return FALSE;
  }

  // Prepare transaction complete process data.

  $settings = commerce_sermepa_get_payment_settings();
  $settings += array(
    // Return to the previous page when payment is canceled.
    'cancel_return' => url('checkout/' . $order->order_id . '/payment/back/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
    // Return to the payment redirect page for processing successful payments.
    'return' => url('checkout/' . $order->order_id . '/payment/return/' . $order->data['payment_redirect_key'], array('absolute' => TRUE)),
    // Url to get POST result of payment.
    'merchant_url' => url('sermepa/callback', array('absolute' => TRUE)),
  );

  // Set Authorization trasaction type.
  // See \facine\Payment\Sermepa::getAvailableTransactionTypes() for full list.
  $gateway->setTransactionType('A')
  // Assign an order number.
          ->setOrder(substr(date('ymdHis') . $order->order_id, -12, 12))
  // Set transaction amount.
          ->setAmount($amount)
  // Set currency.
          ->setCurrency($settings['currency'])
  // Set merchant name.
//          ->setMerchantName($settings['Ds_MerchantName'])
  // Set feedback url.
          ->setMerchantURL($settings['merchant_url'])
  // Set Spanish as default customer language.
  // @todo: Add language settings to payment method and/or current user detection.
          ->setConsumerLanguage('001')
  // Store commerce order id in merchant data.
//         ->setMerchantData($order->order_id)
         ->setMerchantIdentifier($transaction->data['sermepa']['Ds_Merchant_Identifier'])
         ->setDirectPayment('TRUE');

  // Build a name-value pair array for this transaction.
  // Submit the capture request to sermepa.
  $response = commerce_sermepa_payment_reference_complete_request($gateway, $settings);
}
